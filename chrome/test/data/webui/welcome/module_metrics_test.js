// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('onboarding_welcome_module_metrics', function() {
  suite('ModuleMetricsTest', function() {
    /** @type {welcome.ModuleMetricsProxy} */
    let testMetricsProxy;

    /** @type {welcome.ModuleMetricsManager} */
    let testMetricsManager;

    setup(function() {
      testMetricsProxy = new TestMetricsProxy();
      testMetricsManager = new welcome.ModuleMetricsManager(testMetricsProxy);

      testMetricsManager.recordPageInitialized();

      return testMetricsProxy.whenCalled('recordPageShown');
    });

    test('do nothing, click skip', function() {
      testMetricsManager.recordNoThanks();
      return testMetricsProxy.whenCalled('recordDidNothingAndChoseSkip');
    });

    test('do nothing, click next', function() {
      testMetricsManager.recordGetStarted();
      return testMetricsProxy.whenCalled('recordDidNothingAndChoseNext');
    });

    test('do nothing, navigate away', function() {
      testMetricsManager.recordNavigatedAway();
      return testMetricsProxy.whenCalled('recordDidNothingAndNavigatedAway');
    });

    test('choose option, click skip', function() {
      testMetricsManager.recordClickedOption();
      testMetricsManager.recordNoThanks();
      return testMetricsProxy.whenCalled('recordChoseAnOptionAndChoseSkip');
    });

    test('choose option, click next', function() {
      testMetricsManager.recordClickedOption();
      testMetricsManager.recordGetStarted();
      return testMetricsProxy.whenCalled('recordChoseAnOptionAndChoseNext');
    });

    test('choose option, navigate away', function() {
      testMetricsManager.recordClickedOption();
      testMetricsManager.recordNavigatedAway();
      return testMetricsProxy.whenCalled('recordChoseAnOptionAndNavigatedAway');
    });

    test('click disabled next, click skip', function() {
      testMetricsManager.recordClickedDisabledButton();
      testMetricsManager.recordNoThanks();
      return testMetricsProxy.whenCalled(
          'recordClickedDisabledNextButtonAndChoseSkip');
    });

    test('click disabled next, click next', function() {
      testMetricsManager.recordClickedDisabledButton();
      // 'Next' should become enabled only after clicking another option.
      testMetricsManager.recordClickedOption();
      testMetricsManager.recordGetStarted();
      return testMetricsProxy.whenCalled(
          'recordClickedDisabledNextButtonAndChoseNext');
    });

    test('click disabled next, navigate away', function() {
      testMetricsManager.recordClickedDisabledButton();
      testMetricsManager.recordNavigatedAway();
      return testMetricsProxy.whenCalled(
          'recordClickedDisabledNextButtonAndNavigatedAway');
    });

    test('choose option, click disabled next, click next', function() {
      testMetricsManager.recordClickedOption();
      testMetricsManager.recordClickedDisabledButton();
      testMetricsManager.recordGetStarted();
      return testMetricsProxy.whenCalled('recordChoseAnOptionAndChoseNext');
    });
  });
});
