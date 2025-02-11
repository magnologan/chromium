// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_device.h"

#include "base/bind.h"
#include "base/numerics/math_constants.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr/arcore_device/ar_image_transport.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl_thread.h"
#include "chrome/browser/android/vr/arcore_device/arcore_impl.h"
#include "chrome/browser/android/vr/arcore_device/arcore_java_utils.h"
#include "chrome/browser/android/vr/arcore_device/arcore_session_utils.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "ui/display/display.h"

using base::android::JavaRef;

namespace {
constexpr float kDegreesPerRadian = 180.0f / base::kPiFloat;
}  // namespace

namespace device {

namespace {

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(mojom::XRDeviceId device_id,
                                            const gfx::Size& frame_size) {
  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();
  device->id = device_id;
  device->display_name = "ARCore VR Device";
  device->webxr_default_framebuffer_scale = 1.0;
  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->has_position = true;
  device->capabilities->has_external_display = false;
  device->capabilities->can_present = false;
  device->capabilities->can_provide_environment_integration = true;
  device->left_eye = mojom::VREyeParameters::New();
  device->right_eye = nullptr;
  mojom::VREyeParametersPtr& left_eye = device->left_eye;
  left_eye->field_of_view = mojom::VRFieldOfView::New();
  // TODO(lincolnfrog): get these values for real (see gvr device).
  double fov_x = 1437.387;
  double fov_y = 1438.074;
  // TODO(lincolnfrog): get real camera intrinsics.
  int width = frame_size.width();
  int height = frame_size.height();
  float horizontal_degrees = atan(width / (2.0 * fov_x)) * kDegreesPerRadian;
  float vertical_degrees = atan(height / (2.0 * fov_y)) * kDegreesPerRadian;
  left_eye->field_of_view->left_degrees = horizontal_degrees;
  left_eye->field_of_view->right_degrees = horizontal_degrees;
  left_eye->field_of_view->up_degrees = vertical_degrees;
  left_eye->field_of_view->down_degrees = vertical_degrees;
  left_eye->offset = {0.0f, 0.0f, 0.0f};
  left_eye->render_width = width;
  left_eye->render_height = height;
  return device;
}

}  // namespace

ArCoreDevice::SessionState::SessionState() = default;
ArCoreDevice::SessionState::~SessionState() = default;

ArCoreDevice::ArCoreDevice(
    std::unique_ptr<ArCoreFactory> arcore_factory,
    std::unique_ptr<ArImageTransportFactory> ar_image_transport_factory,
    std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_to_surface_bridge,
    std::unique_ptr<vr::ArCoreSessionUtils> arcore_session_utils)
    : VRDeviceBase(mojom::XRDeviceId::ARCORE_DEVICE_ID),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      arcore_factory_(std::move(arcore_factory)),
      ar_image_transport_factory_(std::move(ar_image_transport_factory)),
      mailbox_bridge_(std::move(mailbox_to_surface_bridge)),
      arcore_session_utils_(std::move(arcore_session_utils)),
      session_state_(std::make_unique<ArCoreDevice::SessionState>()),
      weak_ptr_factory_(this) {
  // Ensure display_info_ is set to avoid crash in CallDeferredSessionCallback
  // if initialization fails. Use an arbitrary but really low resolution to make
  // it obvious if we're using this data instead of the actual values we get
  // from the output drawing surface.
  SetVRDisplayInfo(CreateVRDisplayInfo(GetId(), {16, 16}));

  // TODO(https://crbug.com/836524) clean up usage of mailbox bridge
  // and extract the methods in this class that interact with ARCore API
  // into a separate class that implements the ArCore interface.
  mailbox_bridge_->CreateUnboundContextProvider(
      base::BindOnce(&ArCoreDevice::OnMailboxBridgeReady, GetWeakPtr()));
}

ArCoreDevice::ArCoreDevice()
    : ArCoreDevice(std::make_unique<ArCoreImplFactory>(),
                   std::make_unique<ArImageTransportFactory>(),
                   std::make_unique<vr::MailboxToSurfaceBridge>(),
                   std::make_unique<vr::ArCoreJavaUtils>()) {}

ArCoreDevice::~ArCoreDevice() {
  CallDeferredRequestSessionCallback(/*success=*/false);
  // The GL thread must be terminated since it uses our members. For example,
  // there might still be a posted Initialize() call in flight that uses
  // arcore_session_utils_ and arcore_factory_. Ensure that the thread is
  // stopped before other members get destructed. Don't call Stop() here,
  // destruction calls Stop() and doing so twice is illegal (null pointer
  // dereference).
  session_state_->arcore_gl_thread_ = nullptr;
}

void ArCoreDevice::OnMailboxBridgeReady() {
  DVLOG(1) << __func__;
  DCHECK(IsOnMainThread());
  DCHECK(!session_state_->arcore_gl_thread_);
  // MailboxToSurfaceBridge's destructor's call to DestroyContext must
  // happen on the GL thread, so transferring it to that thread is appropriate.
  // TODO(https://crbug.com/836553): use same GL thread as GVR.
  session_state_->arcore_gl_thread_ = std::make_unique<ArCoreGlThread>(
      std::move(ar_image_transport_factory_), std::move(mailbox_bridge_),
      CreateMainThreadCallback(base::BindOnce(
          &ArCoreDevice::OnArCoreGlThreadInitialized, GetWeakPtr())));
  session_state_->arcore_gl_thread_->Start();
}

void ArCoreDevice::OnArCoreGlThreadInitialized() {
  DVLOG(1) << __func__;
  DCHECK(IsOnMainThread());

  session_state_->is_arcore_gl_thread_initialized_ = true;

  if (session_state_->pending_request_session_after_gl_thread_initialized_) {
    std::move(
        session_state_->pending_request_session_after_gl_thread_initialized_)
        .Run();
  }
}

void ArCoreDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  DVLOG(1) << __func__;
  DCHECK(IsOnMainThread());

  if (session_state_->pending_request_session_callback_) {
    DVLOG(1) << __func__ << ": Rejecting additional session request";
    std::move(callback).Run(nullptr, nullptr);
    return;
  }
  session_state_->pending_request_session_callback_ = std::move(callback);

  if (session_state_->is_arcore_gl_thread_initialized_) {
    // First session on a new ArCoreDevice, and it's ready to proceed now.
    RequestSessionAfterInitialization(options->render_process_id,
                                      options->render_frame_id);
  } else {
    if (mailbox_bridge_) {
      // This is a new ArCoreDevice, but its mailbox_bridge_ hasn't finished
      // initialization yet.
    } else {
      // We're reusing a previously constructed ArCoreDevice for a new session.
      // Restart initialization.
      mailbox_bridge_ = std::make_unique<vr::MailboxToSurfaceBridge>();
      mailbox_bridge_->CreateUnboundContextProvider(
          base::BindOnce(&ArCoreDevice::OnMailboxBridgeReady, GetWeakPtr()));
    }

    // We're now expecting a call to OnMailboxBridgeReady() which will create
    // a new GL thread, and at some point after that GL thread initialization
    // will complete which calls OnArCoreGlThreadInitialized().
    session_state_->pending_request_session_after_gl_thread_initialized_ =
        base::BindOnce(&ArCoreDevice::RequestSessionAfterInitialization,
                       GetWeakPtr(), options->render_process_id,
                       options->render_frame_id);
  }
}

void ArCoreDevice::RequestSessionAfterInitialization(int render_process_id,
                                                     int render_frame_id) {
  auto ready_callback =
      base::BindRepeating(&ArCoreDevice::OnDrawingSurfaceReady, GetWeakPtr());
  auto touch_callback =
      base::BindRepeating(&ArCoreDevice::OnDrawingSurfaceTouch, GetWeakPtr());
  auto destroyed_callback =
      base::BindOnce(&ArCoreDevice::OnDrawingSurfaceDestroyed, GetWeakPtr());

  arcore_session_utils_->RequestArSession(
      render_process_id, render_frame_id, std::move(ready_callback),
      std::move(touch_callback), std::move(destroyed_callback));
}

void ArCoreDevice::OnDrawingSurfaceReady(gfx::AcceleratedWidget window,
                                         display::Display::Rotation rotation,
                                         const gfx::Size& frame_size) {
  DVLOG(1) << __func__ << ": size=" << frame_size.width() << "x"
           << frame_size.height() << " rotation=" << static_cast<int>(rotation);
  DCHECK(!session_state_->is_arcore_gl_initialized_);

  auto display_info = CreateVRDisplayInfo(GetId(), frame_size);
  SetVRDisplayInfo(std::move(display_info));

  RequestArCoreGlInitialization(window, rotation, frame_size);
}

void ArCoreDevice::OnDrawingSurfaceTouch(bool touching,
                                         const gfx::PointF& location) {
  DVLOG(2) << __func__ << ": touching=" << touching;

  if (!session_state_->is_arcore_gl_initialized_ ||
      !session_state_->arcore_gl_thread_)
    return;

  PostTaskToGlThread(base::BindOnce(
      &ArCoreGl::OnScreenTouch,
      session_state_->arcore_gl_thread_->GetArCoreGl()->GetWeakPtr(), touching,
      location));
}

void ArCoreDevice::OnDrawingSurfaceDestroyed() {
  DVLOG(1) << __func__;

  CallDeferredRequestSessionCallback(/*success=*/false);

  OnSessionEnded();
}

void ArCoreDevice::OnSessionEnded() {
  DVLOG(1) << __func__;

  // This may be a no-op in case it's destroyed already.
  arcore_session_utils_->DestroyDrawingSurface();

  // The GL thread had initialized its context with a drawing_widget based on
  // the ArImmersiveOverlay's Surface, and the one it has is no longer valid.
  // For now, just destroy the GL thread so that it is recreated for the next
  // session with fresh associated resources. Also go through these steps in
  // case the GL thread hadn't completed, or had initialized partially, to
  // ensure consistent state.

  // TODO(https://crbug.com/849568): Instead of splitting the initialization
  // of this class between construction and RequestSession, perform all the
  // initialization at once on the first successful RequestSession call.

  // Reset per-session members to initial values.
  session_state_ = std::make_unique<ArCoreDevice::SessionState>();

  // The image transport factory should be reusable, but we've std::moved it
  // to the GL thread. Make a new one for next time. (This is cheap, it's
  // just a factory.)
  ar_image_transport_factory_ = std::make_unique<ArImageTransportFactory>();

  // Shut down the mailbox bridge, this has the side effect of also destroying
  // GL resources in the GPU process.
  mailbox_bridge_ = nullptr;
}

void ArCoreDevice::CallDeferredRequestSessionCallback(bool success) {
  DVLOG(1) << __func__ << " success=" << success;
  DCHECK(IsOnMainThread());

  // We might not have any pending session requests, i.e. if destroyed
  // immediately after construction.
  if (!session_state_->pending_request_session_callback_)
    return;

  mojom::XRRuntime::RequestSessionCallback deferred_callback =
      std::move(session_state_->pending_request_session_callback_);

  if (!success) {
    std::move(deferred_callback).Run(nullptr, nullptr);
    return;
  }

  // Success case should only happen after GL thread is ready.
  DCHECK(session_state_->is_arcore_gl_thread_initialized_);
  auto create_callback =
      base::BindOnce(&ArCoreDevice::OnCreateSessionCallback, GetWeakPtr(),
                     std::move(deferred_callback));

  auto shutdown_callback =
      base::BindOnce(&ArCoreDevice::OnSessionEnded, GetWeakPtr());

  PostTaskToGlThread(base::BindOnce(
      &ArCoreGl::CreateSession,
      session_state_->arcore_gl_thread_->GetArCoreGl()->GetWeakPtr(),
      display_info_->Clone(),
      CreateMainThreadCallback(std::move(create_callback)),
      CreateMainThreadCallback(std::move(shutdown_callback))));
}

void ArCoreDevice::OnCreateSessionCallback(
    mojom::XRRuntime::RequestSessionCallback deferred_callback,
    mojom::XRFrameDataProviderPtrInfo frame_data_provider_info,
    mojom::VRDisplayInfoPtr display_info,
    mojom::XRSessionControllerPtrInfo session_controller_info,
    mojom::XRPresentationConnectionPtr presentation_connection) {
  DVLOG(2) << __func__;
  DCHECK(IsOnMainThread());

  mojom::XRSessionPtr session = mojom::XRSession::New();
  session->data_provider = std::move(frame_data_provider_info);
  session->display_info = std::move(display_info);
  session->submit_frame_sink = std::move(presentation_connection);

  mojom::XRSessionControllerPtr controller(std::move(session_controller_info));

  std::move(deferred_callback).Run(std::move(session), std::move(controller));
}

void ArCoreDevice::PostTaskToGlThread(base::OnceClosure task) {
  DCHECK(IsOnMainThread());
  session_state_->arcore_gl_thread_->GetArCoreGl()
      ->GetGlThreadTaskRunner()
      ->PostTask(FROM_HERE, std::move(task));
}

bool ArCoreDevice::IsOnMainThread() {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

void ArCoreDevice::RequestArCoreGlInitialization(
    gfx::AcceleratedWidget drawing_widget,
    int drawing_rotation,
    const gfx::Size& frame_size) {
  DVLOG(1) << __func__;
  DCHECK(IsOnMainThread());
  DCHECK(session_state_->is_arcore_gl_thread_initialized_);

  if (!arcore_session_utils_->EnsureLoaded()) {
    DLOG(ERROR) << "ARCore was not loaded properly.";
    OnArCoreGlInitializationComplete(false);
    return;
  }

  if (!session_state_->is_arcore_gl_initialized_) {
    // We will only try to initialize ArCoreGl once, at the end of the
    // permission sequence, and will resolve pending requests that have queued
    // up once that initialization completes. We set is_arcore_gl_initialized_
    // in the callback to block operations that require it to be ready.
    auto rotation = static_cast<display::Display::Rotation>(drawing_rotation);
    PostTaskToGlThread(base::BindOnce(
        &ArCoreGl::Initialize,
        session_state_->arcore_gl_thread_->GetArCoreGl()->GetWeakPtr(),
        arcore_session_utils_.get(), arcore_factory_.get(), drawing_widget,
        frame_size, rotation,
        CreateMainThreadCallback(base::BindOnce(
            &ArCoreDevice::OnArCoreGlInitializationComplete, GetWeakPtr()))));
    return;
  }

  OnArCoreGlInitializationComplete(true);
}

void ArCoreDevice::OnArCoreGlInitializationComplete(bool success) {
  DVLOG(1) << __func__;
  DCHECK(IsOnMainThread());
  DCHECK(session_state_->is_arcore_gl_thread_initialized_);

  if (!success) {
    CallDeferredRequestSessionCallback(/*success=*/false);
    return;
  }

  session_state_->is_arcore_gl_initialized_ = true;

  // We only start GL initialization after the user has granted consent, so we
  // can now start the session.
  CallDeferredRequestSessionCallback(/*success=*/true);
}

}  // namespace device
