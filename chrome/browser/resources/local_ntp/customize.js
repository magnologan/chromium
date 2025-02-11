// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// TODO(crbug.com/937570): After the RP launches this should be renamed to
// customizationMenu along with the file, and large parts can be
// refactored/removed.
const customize = {};

/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
let ntpApiHandle;

/**
 * The different types of events that are logged from the NTP. This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE = {
  // The 'Chrome backgrounds' menu item was clicked.
  NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED: 40,
  // The 'Upload an image' menu item was clicked.
  NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED: 41,
  // The 'Restore default background' menu item was clicked.
  // NOTE: NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED (42) is logged on the
  // backend.
  // The attribution link on a customized background image was clicked.
  NTP_CUSTOMIZE_ATTRIBUTION_CLICKED: 43,
  // The 'Restore default shortcuts' menu item was clicked.
  NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED: 46,
  // A collection was selected in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION: 47,
  // An image was selected in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE: 48,
  // 'Cancel' was clicked in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL: 49,
  // NOTE: NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE (50) is logged on the backend.
};

/**
 * Enum for key codes.
 * @enum {number}
 * @const
 */
customize.KEYCODES = {
  BACKSPACE: 8,
  DOWN: 40,
  ENTER: 13,
  ESC: 27,
  LEFT: 37,
  RIGHT: 39,
  SPACE: 32,
  TAB: 9,
  UP: 38,
};

/**
 * Array for keycodes corresponding to arrow keys.
 * @type Array
 * @const
 */
customize.arrowKeys = [/*Left*/ 37, /*Up*/ 38, /*Right*/ 39, /*Down*/ 40];

/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
customize.IDS = {
  ATTR1: 'attr1',
  ATTR2: 'attr2',
  ATTRIBUTIONS: 'custom-bg-attr',
  BACK_CIRCLE: 'bg-sel-back-circle',
  BACKGROUNDS_DEFAULT: 'backgrounds-default',
  BACKGROUNDS_DEFAULT_ICON: 'backgrounds-default-icon',
  BACKGROUNDS_BUTTON: 'backgrounds-button',
  BACKGROUNDS_IMAGE_MENU: 'backgrounds-image-menu',
  BACKGROUNDS_MENU: 'backgrounds-menu',
  BACKGROUNDS_UPLOAD: 'backgrounds-upload',
  BACKGROUNDS_UPLOAD_WRAPPER: 'backgrounds-upload-wrapper',
  CANCEL: 'bg-sel-footer-cancel',
  COLORS_BUTTON: 'colors-button',
  COLORS_DEFAULT_ICON: 'colors-default-icon',
  COLORS_THEME: 'colors-theme',
  COLORS_THEME_NAME: 'colors-theme-name',
  COLORS_THEME_UNINSTALL: 'colors-theme-uninstall',
  COLORS_THEME_WEBSTORE_LINK: 'colors-theme-link',
  COLORS_MENU: 'colors-menu',
  CUSTOMIZATION_MENU: 'customization-menu',
  CUSTOM_BG: 'custom-bg',
  CUSTOM_BG_PREVIEW: 'custom-bg-preview',
  CUSTOM_LINKS_RESTORE_DEFAULT: 'custom-links-restore-default',
  CUSTOM_LINKS_RESTORE_DEFAULT_TEXT: 'custom-links-restore-default-text',
  DEFAULT_WALLPAPERS: 'edit-bg-default-wallpapers',
  DEFAULT_WALLPAPERS_TEXT: 'edit-bg-default-wallpapers-text',
  DONE: 'bg-sel-footer-done',
  EDIT_BG: 'edit-bg',
  EDIT_BG_DIALOG: 'edit-bg-dialog',
  EDIT_BG_DIVIDER: 'edit-bg-divider',
  EDIT_BG_ICON: 'edit-bg-icon',
  EDIT_BG_MENU: 'edit-bg-menu',
  EDIT_BG_TEXT: 'edit-bg-text',
  MENU_BACK_CIRCLE: 'menu-back-circle',
  MENU_CANCEL: 'menu-cancel',
  MENU_DONE: 'menu-done',
  MENU_TITLE: 'menu-title',
  LINK_ICON: 'link-icon',
  MENU: 'bg-sel-menu',
  OPTIONS_TITLE: 'edit-bg-title',
  RESTORE_DEFAULT: 'edit-bg-restore-default',
  RESTORE_DEFAULT_TEXT: 'edit-bg-restore-default-text',
  SHORTCUTS_BUTTON: 'shortcuts-button',
  SHORTCUTS_HIDE: 'sh-hide',
  SHORTCUTS_HIDE_TOGGLE: 'sh-hide-toggle',
  SHORTCUTS_MENU: 'shortcuts-menu',
  SHORTCUTS_OPTION_CUSTOM_LINKS: 'sh-option-cl',
  SHORTCUTS_OPTION_MOST_VISITED: 'sh-option-mv',
  UPLOAD_IMAGE: 'edit-bg-upload-image',
  UPLOAD_IMAGE_TEXT: 'edit-bg-upload-image-text',
  TILES: 'bg-sel-tiles',
  TITLE: 'bg-sel-title',
};

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
customize.CLASSES = {
  ATTR_SMALL: 'attr-small',
  ATTR_COMMON: 'attr-common',
  ATTR_LINK: 'attr-link',
  COLLECTION_DIALOG: 'is-col-sel',
  COLLECTION_SELECTED: 'bg-selected',  // Highlight selected tile
  COLLECTION_TILE: 'bg-sel-tile',  // Preview tile for background customization
  COLLECTION_TILE_BG: 'bg-sel-tile-bg',
  COLLECTION_TITLE: 'bg-sel-tile-title',  // Title of a background image
  // Extended and elevated style for entry point.
  ENTRY_POINT_ENHANCED: 'ep-enhanced',
  IMAGE_DIALOG: 'is-img-sel',
  ON_IMAGE_MENU: 'on-img-menu',
  OPTION: 'bg-option',
  OPTION_DISABLED: 'bg-option-disabled',  // The menu option is disabled.
  MENU_SHOWN: 'menu-shown',
  MOUSE_NAV: 'using-mouse-nav',
  SELECTED: 'selected',
  SELECTED_BORDER: 'selected-border',
  SELECTED_CHECK: 'selected-check',
  SELECTED_CIRCLE: 'selected-circle',
  SINGLE_ATTR: 'single-attr',
  VISIBLE: 'visible'
};

/**
 * Enum for background option menu entries, in the order they appear in the UI.
 * @enum {number}
 * @const
 */
customize.MENU_ENTRIES = {
  CHROME_BACKGROUNDS: 0,
  UPLOAD_IMAGE: 1,
  CUSTOM_LINKS_RESTORE_DEFAULT: 2,
  RESTORE_DEFAULT: 3,
};

/**
 * The semi-transparent, gradient overlay for the custom background. Intended
 * to improve readability of NTP elements/text.
 * @type {string}
 * @const
 */
customize.CUSTOM_BACKGROUND_OVERLAY =
    'linear-gradient(rgba(0, 0, 0, 0), rgba(0, 0, 0, 0.3))';

/**
 * Number of rows in the custom background dialog to preload.
 * @type {number}
 * @const
 */
customize.ROWS_TO_PRELOAD = 3;

// These should match the corresponding values in local_ntp.js, that control the
// mv-notice element.
customize.delayedHideNotification = -1;
customize.NOTIFICATION_TIMEOUT = 10000;

/**
 * Were the background tiles already created.
 * @type {boolean}
 */
customize.builtTiles = false;

/**
 * The default title for the richer picker.
 * @type {string}
 */
customize.richerPicker_defaultTitle = '';

/**
 * Called when the error notification should be shown.
 * @type {?Function}
 */
customize.showErrorNotification = null;

/**
 * Called when the custom link notification should be hidden.
 * @type {?Function}
 */
customize.hideCustomLinkNotification = null;

/**
 * The currently active Background submenu. This can be the collections page or
 * a collection's image menu. Defaults to the collections page.
 * @type {Object}
 */
customize.richerPicker_openBackgroundSubmenu = {
  menuId: customize.IDS.BACKGROUNDS_MENU,
  title: '',
};

/**
 * The currently selected submenu (i.e. Background, Shortcuts, etc.) in the
 * richer picker.
 * @type {Object}
 */
customize.richerPicker_selectedSubmenu = {
  menuButton: null,  // The submenu's button element in the sidebar.
  menu: null,        // The submenu's menu element.
  // The submenu's title. Will usually be |customize.richerPicker_defaultTitle|
  // unless this is a background collection's image menu.
  title: '',
};

/**
 * The currently selected options in the customization menu.
 * @type {Object}
 */
customize.selectedOptions = {
  background: null,  // Contains the background image tile.
  // The data associated with a currently selected background.
  backgroundData: null,
  // Contains the selected shortcut type's DOM element, i.e. either custom links
  // or most visited.
  shortcutType: null,
  shortcutsAreHidden: false,
  color: null,  // Contains the selected color tile's DOM element.
};

/**
 * The preselected options for Shortcuts in the richer picker.
 * @type {Object}
 */
customize.preselectedShortcutOptions = {
  // Contains the selected type's DOM element, i.e. either custom links or most
  // visited.
  shortcutType: null,
  shortcutsAreHidden: false,
};

/**
 * Whether tiles for Colors menu already loaded.
 * @type {boolean}
 */
customize.colorsMenuLoaded = false;

/**
 * Sets the visibility of the settings menu and individual options depending on
 * their respective features.
 */
customize.setMenuVisibility = function() {
  // Reset all hidden values.
  $(customize.IDS.EDIT_BG).hidden = false;
  $(customize.IDS.DEFAULT_WALLPAPERS).hidden = false;
  $(customize.IDS.UPLOAD_IMAGE).hidden = false;
  $(customize.IDS.RESTORE_DEFAULT).hidden = false;
  $(customize.IDS.EDIT_BG_DIVIDER).hidden = false;
  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).hidden =
      configData.hideShortcuts;
  $(customize.IDS.COLORS_BUTTON).hidden = !configData.chromeColors;
};

/**
 * Called when user changes the theme.
 */
customize.onThemeChange = function() {
  // Hide the settings menu or individual options if the related features are
  // disabled.
  customize.setMenuVisibility();

  // If theme changed after Colors menu was loaded, then reload theme info.
  if (customize.colorsMenuLoaded) {
    customize.updateWebstoreThemeInfo();
  }
};

/**
 * Display custom background image attributions on the page.
 * @param {string} attributionLine1 First line of attribution.
 * @param {string} attributionLine2 Second line of attribution.
 * @param {string} attributionActionUrl Url to learn more about the image.
 */
customize.setAttribution = function(
    attributionLine1, attributionLine2, attributionActionUrl) {
  const attributionBox = $(customize.IDS.ATTRIBUTIONS);
  const attr1 = document.createElement('span');
  attr1.id = customize.IDS.ATTR1;
  const attr2 = document.createElement('span');
  attr2.id = customize.IDS.ATTR2;

  if (attributionLine1 !== '') {
    // Shouldn't be changed from textContent for security assurances.
    attr1.textContent = attributionLine1;
    attr1.classList.add(customize.CLASSES.ATTR_COMMON);
    $(customize.IDS.ATTRIBUTIONS).appendChild(attr1);
  }
  if (attributionLine2 !== '') {
    // Shouldn't be changed from textContent for security assurances.
    attr2.textContent = attributionLine2;
    attr2.classList.add(customize.CLASSES.ATTR_SMALL);
    attr2.classList.add(customize.CLASSES.ATTR_COMMON);
    attributionBox.appendChild(attr2);
  }
  if (attributionActionUrl !== '') {
    const attr = (attributionLine2 !== '' ? attr2 : attr1);
    attr.classList.add(customize.CLASSES.ATTR_LINK);

    const linkIcon = document.createElement('div');
    linkIcon.id = customize.IDS.LINK_ICON;
    // Enlarge link-icon when there is only one line of attribution
    if (attributionLine2 === '') {
      linkIcon.classList.add(customize.CLASSES.SINGLE_ATTR);
    }
    attr.insertBefore(linkIcon, attr.firstChild);

    attributionBox.classList.add(customize.CLASSES.ATTR_LINK);
    attributionBox.href = attributionActionUrl;
    attributionBox.onclick = function() {
      ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                                .NTP_CUSTOMIZE_ATTRIBUTION_CLICKED);
    };
    attributionBox.style.cursor = 'pointer';
  }
};

customize.clearAttribution = function() {
  const attributions = $(customize.IDS.ATTRIBUTIONS);
  attributions.removeAttribute('href');
  attributions.className = '';
  attributions.style.cursor = 'default';
  while (attributions.firstChild) {
    attributions.removeChild(attributions.firstChild);
  }
};

customize.unselectTile = function() {
  $(customize.IDS.DONE).disabled = true;
  customize.selectedOptions.background = null;
  $(customize.IDS.DONE).tabIndex = -1;
};

/**
 * Remove all collection tiles from the container when the dialog
 * is closed.
 */
customize.resetSelectionDialog = function() {
  $(customize.IDS.TILES).scrollTop = 0;
  const tileContainer = $(customize.IDS.TILES);
  while (tileContainer.firstChild) {
    tileContainer.removeChild(tileContainer.firstChild);
  }
  customize.unselectTile();
};

/**
 * Apply selected styling to |menuButton| and make corresponding |menu| visible.
 * If |title| is not specified, the default title will be used.
 * @param {?Element} menuButton The sidebar button element to apply styling to.
 * @param {?Element} menu The submenu element to apply styling to.
 * @param {string=} title The submenu's title.
 */
customize.richerPicker_showSubmenu = function(menuButton, menu, title = '') {
  if (!menuButton || !menu) {
    return;
  }

  customize.richerPicker_hideOpenSubmenu();

  if (!title) {  // Use the default title if not specified.
    title = customize.richerPicker_defaultTitle;
  }

  // Save this as the currently open submenu.
  customize.richerPicker_selectedSubmenu.menuButton = menuButton;
  customize.richerPicker_selectedSubmenu.menu = menu;
  customize.richerPicker_selectedSubmenu.title = title;

  menuButton.classList.toggle(customize.CLASSES.SELECTED, true);
  menu.classList.toggle(customize.CLASSES.MENU_SHOWN, true);
  $(customize.IDS.MENU_TITLE).textContent = title;
  menuButton.setAttribute('aria-selected', true);

  // Indicate if this is a Background collection's image menu, which will enable
  // the back button.
  $(customize.IDS.CUSTOMIZATION_MENU)
      .classList.toggle(
          customize.CLASSES.ON_IMAGE_MENU,
          menu.id === customize.IDS.BACKGROUNDS_IMAGE_MENU);
};

/**
 * Hides the currently open submenu if any.
 */
customize.richerPicker_hideOpenSubmenu = function() {
  if (!customize.richerPicker_selectedSubmenu.menuButton) {
    return;  // No submenu is open.
  }

  customize.richerPicker_selectedSubmenu.menuButton.classList.toggle(
      customize.CLASSES.SELECTED, false);
  customize.richerPicker_selectedSubmenu.menu.classList.toggle(
      customize.CLASSES.MENU_SHOWN, false);
  $(customize.IDS.MENU_TITLE).textContent = customize.richerPicker_defaultTitle;
  customize.richerPicker_selectedSubmenu.menuButton.setAttribute(
      'aria-selected', false);

  customize.richerPicker_selectedSubmenu.menuButton = null;
  customize.richerPicker_selectedSubmenu.menu = null;
  customize.richerPicker_selectedSubmenu.title =
      customize.richerPicker_defaultTitle;
};

/**
 * Remove image tiles and maybe swap back to main background menu.
 */
customize.richerPicker_resetImageMenu = function() {
  const backgroundMenu = $(customize.IDS.BACKGROUNDS_MENU);
  const imageMenu = $(customize.IDS.BACKGROUNDS_IMAGE_MENU);
  const menu = $(customize.IDS.CUSTOMIZATION_MENU);
  const menuTitle = $(customize.IDS.MENU_TITLE);

  imageMenu.innerHTML = '';
  menu.classList.toggle(customize.CLASSES.ON_IMAGE_MENU, false);
  customize.richerPicker_showSubmenu(
      $(customize.IDS.BACKGROUNDS_BUTTON), backgroundMenu);
  customize.richerPicker_openBackgroundSubmenu.menuId =
      customize.IDS.BACKGROUNDS_MENU;
  customize.richerPicker_openBackgroundSubmenu.title = '';
  backgroundMenu.scrollTop = 0;
};

/**
 * Close the collection selection dialog and cleanup the state
 * @param {?Element} menu The dialog to be closed
 */
customize.closeCollectionDialog = function(menu) {
  if (!menu) {
    return;
  }
  menu.close();
  customize.resetSelectionDialog();
};

/**
 * Close and reset the dialog if this is not the richer picker, and set the
 * background.
 * @param {string} url The url of the selected background.
 */
customize.setBackground = function(
    url, attributionLine1, attributionLine2, attributionActionUrl) {
  if (!configData.richerPicker) {
    customize.closeCollectionDialog($(customize.IDS.MENU));
  }
  window.chrome.embeddedSearch.newTabPage.setBackgroundURLWithAttributions(
      url, attributionLine1, attributionLine2, attributionActionUrl);
};

/**
 * Apply selected shortcut options.
 */
customize.richerPicker_setShortcutOptions = function() {
  const shortcutTypeChanged =
      customize.preselectedShortcutOptions.shortcutType !==
      customize.selectedOptions.shortcutType;
  if (customize.preselectedShortcutOptions.shortcutsAreHidden !==
      customize.selectedOptions.shortcutsAreHidden) {
    // Only trigger a notification if |toggleMostVisitedOrCustomLinks| will not
    // be called immediately after. Successive |onmostvisitedchange| events can
    // interfere with each other.
    chrome.embeddedSearch.newTabPage.toggleShortcutsVisibility(
        !shortcutTypeChanged);
  }
  if (shortcutTypeChanged) {
    chrome.embeddedSearch.newTabPage.toggleMostVisitedOrCustomLinks();
  }
};

/**
 * Creates a tile for the customization menu with a title.
 * @param {string} id The id for the new element.
 * @param {string} imageUrl The background image url for the new element.
 * @param {string} name The name for the title of the new element.
 * @param {Object} dataset The dataset for the new element.
 * @param {?Function} onClickInteraction Function for onclick interaction.
 * @param {?Function} onKeyInteraction Function for onkeydown interaction.
 */
customize.createTileWithTitle = function(
    id, imageUrl, name, dataset, onClickInteraction, onKeyInteraction) {
  const tile = customize.createTileThumbnail(
      id, imageUrl, dataset, onClickInteraction, onKeyInteraction);
  customize.fadeInImageTile(tile, imageUrl, null);

  const title = document.createElement('div');
  title.classList.add(customize.CLASSES.COLLECTION_TITLE);
  title.textContent = name;
  tile.appendChild(title);

  const tileBackground = document.createElement('div');
  tileBackground.classList.add(customize.CLASSES.COLLECTION_TILE_BG);
  tileBackground.appendChild(tile);
  return tileBackground;
};

/**
 * Creates a tile for the customization menu without a title.
 * @param {string} id The id for the new element.
 * @param {string} imageUrl The background image url for the new element.
 * @param {Object} dataset The dataset for the new element.
 * @param {?Function} onClickInteraction Function for onclick interaction.
 * @param {?Function} onKeyInteraction Function for onkeydown interaction.
 */
customize.createTileWithoutTitle = function(
    id, imageUrl, dataset, onClickInteraction, onKeyInteraction) {
  const tile = customize.createTileThumbnail(
      id, imageUrl, dataset, onClickInteraction, onKeyInteraction);
  customize.fadeInImageTile(tile, imageUrl, null);

  const tileBackground = document.createElement('div');
  tileBackground.classList.add(customize.CLASSES.COLLECTION_TILE_BG);
  tileBackground.appendChild(tile);
  return tileBackground;
};

/**
 * Create a tile thumbnail with image for customization menu.
 * @param {string} id The id for the new element.
 * @param {string} imageUrl The background image url for the new element.
 * @param {Object} dataset The dataset for the new element.
 * @param {?Function} onClickInteraction Function for onclick interaction.
 * @param {?Function} onKeyInteraction Function for onkeydown interaction.
 */
customize.createTileThumbnail = function(
    id, imageUrl, dataset, onClickInteraction, onKeyInteraction) {
  const tile = document.createElement('div');
  tile.id = id;
  tile.classList.add(customize.CLASSES.COLLECTION_TILE);
  tile.style.backgroundImage = 'url(' + imageUrl + ')';
  for (const key in dataset) {
    tile.dataset[key] = dataset[key];
  }
  tile.tabIndex = -1;

  // Accessibility support for screen readers.
  tile.setAttribute('role', 'button');

  tile.onclick = onClickInteraction;
  tile.onkeydown = onKeyInteraction;
  return tile;
};

/**
 * Get the number of tiles in a row according to current window width.
 * @return {number} the number of tiles per row
 */
customize.getTilesWide = function() {
  // Browser window can only fit two columns. Should match "#bg-sel-menu" width.
  if ($(customize.IDS.MENU).offsetWidth < 517) {
    return 2;
  } else if ($(customize.IDS.MENU).offsetWidth < 356) {
    // Browser window can only fit one column. Should match @media (max-width:
    // 356) "#bg-sel-menu" width.
    return 1;
  }

  return 3;
};

/**
 * @param {number} deltaX Change in the x direction.
 * @param {number} deltaY Change in the y direction.
 * @param {Element} current The current tile.
 */
customize.richerPicker_getNextTile = function(deltaX, deltaY, current) {
  const menu = $(customize.IDS.CUSTOMIZATION_MENU);
  const container = menu.classList.contains(customize.CLASSES.ON_IMAGE_MENU) ?
      $(customize.IDS.BACKGROUNDS_IMAGE_MENU) :
      $(customize.IDS.BACKGROUNDS_MENU);
  const tiles = Array.from(
      container.getElementsByClassName(customize.CLASSES.COLLECTION_TILE));
  const curIndex = tiles.indexOf(current);
  if (deltaX != 0) {
    return tiles[curIndex + deltaX];
  } else if (deltaY != 0) {
    let nextIndex = tiles.indexOf(current);
    const startingTop = current.getBoundingClientRect().top;
    const startingLeft = current.getBoundingClientRect().left;

    // Search until a tile in a different row and the same column is found.
    while (tiles[nextIndex] &&
           (tiles[nextIndex].getBoundingClientRect().top == startingTop ||
            tiles[nextIndex].getBoundingClientRect().left != startingLeft)) {
      nextIndex += deltaY;
    }
    return tiles[nextIndex];
  }
  return null;
};

/**
 * Get the next tile when the arrow keys are used to navigate the grid.
 * Returns null if the tile doesn't exist.
 * @param {number} deltaX Change in the x direction.
 * @param {number} deltaY Change in the y direction.
 * @param {Element} currentElem The current tile.
 */
customize.getNextTile = function(deltaX, deltaY, currentElem) {
  if (configData.richerPicker) {
    return customize.richerPicker_getNextTile(deltaX, deltaY, currentElem);
  }
  const current = currentElem.dataset.tileIndex;
  let idPrefix = 'coll_tile_';
  if ($(customize.IDS.MENU)
          .classList.contains(customize.CLASSES.IMAGE_DIALOG)) {
    idPrefix = 'img_tile_';
  }

  if (deltaX != 0) {
    const target = parseInt(current, /*radix=*/ 10) + deltaX;
    return $(idPrefix + target);
  } else if (deltaY != 0) {
    let target = parseInt(current, /*radix=*/ 10);
    let nextTile = $(idPrefix + target);
    const startingTop = nextTile.getBoundingClientRect().top;
    const startingLeft = nextTile.getBoundingClientRect().left;

    // Search until a tile in a different row and the same column is found.
    while (nextTile &&
           (nextTile.getBoundingClientRect().top == startingTop ||
            nextTile.getBoundingClientRect().left != startingLeft)) {
      target += deltaY;
      nextTile = $(idPrefix + target);
    }
    return nextTile;
  }
};

/**
 * Show dialog for selecting a Chrome background.
 */
customize.showCollectionSelectionDialog = function() {
  const tileContainer = configData.richerPicker ?
      $(customize.IDS.BACKGROUNDS_MENU) :
      $(customize.IDS.TILES);
  if (configData.richerPicker && customize.builtTiles) {
    return;
  }
  customize.builtTiles = true;
  const menu = configData.richerPicker ? $(customize.IDS.CUSTOMIZATION_MENU) :
                                         $(customize.IDS.MENU);
  if (!menu.open) {
    menu.showModal();
  }

  // Create dialog header.
  $(customize.IDS.TITLE).textContent =
      configData.translatedStrings.selectChromeWallpaper;
  if (!configData.richerPicker) {
    menu.classList.add(customize.CLASSES.COLLECTION_DIALOG);
    menu.classList.remove(customize.CLASSES.IMAGE_DIALOG);
  }

  const tileOnClickInteraction = function(event) {
    let tile = event.target;
    if (tile.classList.contains(customize.CLASSES.COLLECTION_TITLE)) {
      tile = tile.parentNode;
    }

    // Load images for selected collection.
    const imgElement = $('ntp-images-loader');
    if (imgElement) {
      imgElement.parentNode.removeChild(imgElement);
    }
    const imgScript = document.createElement('script');
    imgScript.id = 'ntp-images-loader';
    imgScript.src = 'chrome-search://local-ntp/ntp-background-images.js?' +
        'collection_id=' + tile.dataset.id;
    ntpApiHandle.logEvent(
        customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
            .NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION);

    document.body.appendChild(imgScript);

    imgScript.onload = function() {
      // Verify that the individual image data was successfully loaded.
      const imageDataLoaded =
          (collImg.length > 0 && collImg[0].collectionId == tile.dataset.id);

      // Dependent upon the success of the load, populate the image selection
      // dialog or close the current dialog.
      if (imageDataLoaded) {
        $(customize.IDS.BACKGROUNDS_MENU)
            .classList.toggle(customize.CLASSES.MENU_SHOWN, false);
        $(customize.IDS.BACKGROUNDS_IMAGE_MENU)
            .classList.toggle(customize.CLASSES.MENU_SHOWN, true);

        // In the RP the upload or default tile may be selected.
        if (configData.richerPicker) {
          customize.richerPicker_deselectBackgroundTile(
              customize.selectedOptions.background);
        } else {
          customize.resetSelectionDialog();
        }
        customize.showImageSelectionDialog(
            tile.dataset.name, tile.dataset.tileIndex);
      } else {
        customize.handleError(collImgErrors);
      }
    };
  };

  const tileOnKeyDownInteraction = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      event.preventDefault();
      event.stopPropagation();
      if (event.currentTarget.onClickOverride) {
        event.currentTarget.onClickOverride(event);
        return;
      }
      tileOnClickInteraction(event);
    } else if (customize.arrowKeys.includes(event.keyCode)) {
      // Handle arrow key navigation.
      event.preventDefault();
      event.stopPropagation();

      let target = null;
      if (event.keyCode === customize.KEYCODES.LEFT) {
        target = customize.getNextTile(
            document.documentElement.classList.contains('rtl') ? 1 : -1, 0,
            event.currentTarget);
      } else if (event.keyCode === customize.KEYCODES.UP) {
        target = customize.getNextTile(0, -1, event.currentTarget);
      } else if (event.keyCode === customize.KEYCODES.RIGHT) {
        target = customize.getNextTile(
            document.documentElement.classList.contains('rtl') ? -1 : 1, 0,
            event.currentTarget);
      } else if (event.keyCode === customize.KEYCODES.DOWN) {
        target = customize.getNextTile(0, 1, event.currentTarget);
      }
      if (target) {
        target.focus();
      } else {
        event.currentTarget.focus();
      }
    }
  };

  // Create dialog tiles.
  for (let i = 0; i < coll.length; ++i) {
    const id = coll[i].collectionId;
    const name = coll[i].collectionName;
    const imageUrl = coll[i].previewImageUrl;
    const dataset = {'id': id, 'name': name, 'tileIndex': i};

    const tile = customize.createTileWithTitle(
        'coll_tile_' + i, imageUrl, name, dataset, tileOnClickInteraction,
        tileOnKeyDownInteraction);
    tileContainer.appendChild(tile);
  }

  // Attach event listeners for upload and default tiles
  $(customize.IDS.BACKGROUNDS_UPLOAD_WRAPPER).onkeydown =
      tileOnKeyDownInteraction;
  $(customize.IDS.BACKGROUNDS_DEFAULT_ICON).onkeydown =
      tileOnKeyDownInteraction;
  $(customize.IDS.BACKGROUNDS_UPLOAD_WRAPPER).onClickOverride =
      $(customize.IDS.BACKGROUNDS_UPLOAD).onkeydown;
  $(customize.IDS.BACKGROUNDS_DEFAULT_ICON).onClickOverride =
      $(customize.IDS.BACKGROUNDS_DEFAULT).onkeydown;

  $(customize.IDS.TILES).focus();
};

/**
 * Return true if any shortcut option is selected.
 * Note: Shortcut options are preselected according to current user settings.
 */
customize.richerPicker_isShortcutOptionSelected = function() {
  // Check if the currently selected options are not the preselection.
  const notPreselectedType =
      customize.preselectedShortcutOptions.shortcutType !==
      customize.selectedOptions.shortcutType;
  const notPreselectedHidden =
      customize.preselectedShortcutOptions.shortcutsAreHidden !==
      customize.selectedOptions.shortcutsAreHidden;
  return notPreselectedType || notPreselectedHidden;
};

/**
 * Return true if any option is selected. Used to enable the 'done' button.
 */
customize.richerPicker_isOptionSelected = function() {
  return !!customize.selectedOptions.background ||
      !!customize.selectedOptions.color ||
      customize.richerPicker_isShortcutOptionSelected();
};

/**
 * Enable the 'done' button if any option is selected. If no option is selected,
 * disable the 'done' button.
 */
customize.richerPicker_maybeToggleDone = function() {
  const enable = customize.richerPicker_isOptionSelected();
  $(customize.IDS.MENU_DONE).disabled = !enable;
  $(customize.IDS.MENU_DONE).tabIndex = enable ? 1 : 0;
};

/**
 * Apply styling to a selected option in the richer picker (i.e. the selected
 * background image, shortcut type, and color).
 * @param {?Element} option The option to apply styling to.
 */
customize.richerPicker_applySelectedState = function(option) {
  if (!option) {
    return;
  }

  option.parentElement.classList.toggle(customize.CLASSES.SELECTED, true);
  // Create and append a blue checkmark to the selected option.
  const selectedCircle = document.createElement('div');
  const selectedCheck = document.createElement('div');
  selectedCircle.classList.add(customize.CLASSES.SELECTED_CIRCLE);
  selectedCheck.classList.add(customize.CLASSES.SELECTED_CHECK);
  option.appendChild(selectedCircle);
  option.appendChild(selectedCheck);
  option.setAttribute('aria-pressed', true);
};

/**
 * Remove styling from a selected option in the richer picker (i.e. the selected
 * background image, shortcut type, and color).
 * @param {?Element} option The option to remove styling from.
 */
customize.richerPicker_removeSelectedState = function(option) {
  if (!option) {
    return;
  }

  option.parentElement.classList.toggle(customize.CLASSES.SELECTED, false);
  // Remove all blue checkmarks from the selected option (this includes the
  // checkmark and the encompassing circle).
  const select = option.querySelectorAll(
      '.' + customize.CLASSES.SELECTED_CHECK + ', .' +
      customize.CLASSES.SELECTED_CIRCLE);
  select.forEach((element) => {
    element.remove();
  });
  option.setAttribute('aria-pressed', false);
};

/**
 * Preview an image as a custom backgrounds.
 * @param {!Element} tile The tile that was selected.
 */
customize.richerPicker_previewImage = function(tile) {
  // Set preview images at 720p by replacing the params in the url.
  const background = $(customize.IDS.CUSTOM_BG);
  const preview = $(customize.IDS.CUSTOM_BG_PREVIEW);
  if (tile.id !== customize.IDS.BACKGROUNDS_DEFAULT_ICON) {
    preview.dataset.hasImage = true;

    const re = /w\d+\-h\d+/;
    preview.style.backgroundImage =
        tile.style.backgroundImage.replace(re, 'w1280-h720');
  } else {
    preview.dataset.hasImage = false;
    preview.style.backgroundImage = '';
    preview.style.backgroundColor = document.body.style.backgroundColor;
  }
  background.style.opacity = 0;
  preview.style.opacity = 1;
  preview.dataset.hasPreview = true;

  ntpApiHandle.onthemechange();
};

/**
 * Remove a preview image of a custom backgrounds.
 */
customize.richerPicker_unpreviewImage = function() {
  const preview = $(customize.IDS.CUSTOM_BG_PREVIEW);
  preview.style.opacity = 0;
  preview.style.backgroundImage = '';
  preview.style.backgroundColor = 'transparent';
  preview.dataset.hasPreview = false;
  $(customize.IDS.CUSTOM_BG).style.opacity = 1;

  ntpApiHandle.onthemechange();
};

/**
 * Handles background selection. Apply styling to the selected background tile
 * in the richer picker, preview the background, and enable the done button.
 * @param {?Element} tile The selected background tile.
 */
customize.richerPicker_selectBackgroundTile = function(tile) {
  if (!tile) {
    return;
  }

  // Deselect any currently selected tile. If it was the clicked tile don't
  // reselect it.
  if (customize.selectedOptions.background) {
    const id = customize.selectedOptions.background.id;
    customize.richerPicker_deselectBackgroundTile(
        customize.selectedOptions.background);
    if (id === tile.id) {
      return;
    }
  }

  customize.selectedOptions.background = tile;
  customize.selectedOptions.backgroundData = {
    id: tile.id,
    url: tile.dataset.url,
    attr1: tile.dataset.attributionLine1,
    attr2: tile.dataset.attributionLine2,
    attrUrl: tile.dataset.attributionActionUrl,
  };
  customize.richerPicker_applySelectedState(tile);
  customize.richerPicker_maybeToggleDone();
  customize.richerPicker_previewImage(tile);
};

/**
 * Handles background deselection. Remove selected styling from the background
 * tile, unpreview the background, and disable the done button.
 * @param {?Element} tile The background tile to deselect.
 */
customize.richerPicker_deselectBackgroundTile = function(tile) {
  if (!tile) {
    return;
  }
  customize.selectedOptions.background = null;
  customize.selectedOptions.backgroundData = null;
  customize.richerPicker_removeSelectedState(tile);
  customize.richerPicker_maybeToggleDone();
  customize.richerPicker_unpreviewImage();
};

/**
 * Handles shortcut type selection. Apply styling to a selected shortcut option
 * and enable the done button.
 * @param {?Element} shortcutType The shortcut type option's element.
 */
customize.richerPicker_selectShortcutType = function(shortcutType) {
  if (!shortcutType ||
      customize.selectedOptions.shortcutType === shortcutType) {
    return;  // The option has already been selected.
  }

  // Clear the previous selection, if any.
  if (customize.selectedOptions.shortcutType) {
    customize.richerPicker_removeSelectedState(
        customize.selectedOptions.shortcutType);
  }
  customize.selectedOptions.shortcutType = shortcutType;
  customize.richerPicker_applySelectedState(shortcutType);
  customize.richerPicker_maybeToggleDone();
};

/**
 * Handles hide shortcuts toggle. Apply/remove styling for the toggle and
 * enable/disable the done button.
 * @param {boolean} areHidden True if the shortcuts are hidden, i.e. the toggle
 *     is on.
 */
customize.richerPicker_toggleShortcutHide = function(areHidden) {
  // (De)select the shortcut hide option.
  $(customize.IDS.SHORTCUTS_HIDE)
      .classList.toggle(customize.CLASSES.SELECTED, areHidden);
  $(customize.IDS.SHORTCUTS_HIDE_TOGGLE).checked = areHidden;

  customize.selectedOptions.shortcutsAreHidden = areHidden;
  customize.richerPicker_maybeToggleDone();
};

/**
 * Apply border and checkmark when a tile is selected
 * @param {!Element} tile The tile to apply styling to.
 */
customize.applySelectedState = function(tile) {
  tile.classList.add(customize.CLASSES.COLLECTION_SELECTED);
  const selectedBorder = document.createElement('div');
  const selectedCircle = document.createElement('div');
  const selectedCheck = document.createElement('div');
  selectedBorder.classList.add(customize.CLASSES.SELECTED_BORDER);
  selectedCircle.classList.add(customize.CLASSES.SELECTED_CIRCLE);
  selectedCheck.classList.add(customize.CLASSES.SELECTED_CHECK);
  selectedBorder.appendChild(selectedCircle);
  selectedBorder.appendChild(selectedCheck);
  tile.appendChild(selectedBorder);
  tile.dataset.oldLabel = tile.getAttribute('aria-label');
  tile.setAttribute(
      'aria-label',
      tile.dataset.oldLabel + ' ' + configData.translatedStrings.selectedLabel);
};

/**
 * Remove border and checkmark when a tile is un-selected
 * @param {!Element} tile The tile to remove styling from.
 */
customize.removeSelectedState = function(tile) {
  tile.classList.remove(customize.CLASSES.COLLECTION_SELECTED);
  tile.removeChild(tile.firstChild);
  tile.setAttribute('aria-label', tile.dataset.oldLabel);
};

/**
 * Show dialog for selecting an image. Image data should previously have been
 * loaded into collImg via
 * chrome-search://local-ntp/ntp-background-images.js?collection_id=<collection_id>
 * @param {string} dialogTitle The title to be displayed at the top of the
 *     dialog.
 * @param {number} collIndex The index of the collection this image menu belongs
 * to.
 */
customize.showImageSelectionDialog = function(dialogTitle, collIndex) {
  const firstNTile = customize.ROWS_TO_PRELOAD * customize.getTilesWide();
  const tileContainer = configData.richerPicker ?
      $(customize.IDS.BACKGROUNDS_IMAGE_MENU) :
      $(customize.IDS.TILES);
  const menu = configData.richerPicker ? $(customize.IDS.CUSTOMIZATION_MENU) :
                                         $(customize.IDS.MENU);

  if (configData.richerPicker) {
    menu.classList.toggle(customize.CLASSES.ON_IMAGE_MENU, true);
    customize.richerPicker_showSubmenu(
        $(customize.IDS.BACKGROUNDS_BUTTON), tileContainer, dialogTitle);
    // Save the current image menu. Used to restore to when the Background
    // submenu is reopened.
    customize.richerPicker_openBackgroundSubmenu.menuId =
        customize.IDS.BACKGROUNDS_IMAGE_MENU;
    customize.richerPicker_openBackgroundSubmenu.title = dialogTitle;
  } else {
    $(customize.IDS.TITLE).textContent = dialogTitle;
    menu.classList.remove(customize.CLASSES.COLLECTION_DIALOG);
    menu.classList.add(customize.CLASSES.IMAGE_DIALOG);
  }

  const tileInteraction = function(tile) {
    if (customize.selectedOptions.background && !configData.richerPicker) {
      customize.removeSelectedState(customize.selectedOptions.background);
      if (customize.selectedOptions.background.id === tile.id) {
        customize.unselectTile();
        return;
      }
    }

    if (configData.richerPicker) {
      customize.richerPicker_selectBackgroundTile(tile);
    } else {
      customize.applySelectedState(tile);
      customize.selectedOptions.background = tile;
    }

    $(customize.IDS.DONE).tabIndex = 0;

    // Turn toggle off when an image is selected.
    $(customize.IDS.DONE).disabled = false;
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE);
  };

  const tileOnClickInteraction = function(event) {
    const clickCount = event.detail;
    // Control + option + space will fire the onclick event with 0 clickCount.
    if (clickCount <= 1) {
      tileInteraction(event.currentTarget);
    } else if (
        clickCount === 2 &&
        customize.selectedOptions.background === event.currentTarget) {
      customize.setBackground(
          event.currentTarget.dataset.url,
          event.currentTarget.dataset.attributionLine1,
          event.currentTarget.dataset.attributionLine2,
          event.currentTarget.dataset.attributionActionUrl);
    }
  };

  const tileOnKeyDownInteraction = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      event.preventDefault();
      event.stopPropagation();
      tileInteraction(event.currentTarget);
    } else if (customize.arrowKeys.includes(event.keyCode)) {
      // Handle arrow key navigation.
      event.preventDefault();
      event.stopPropagation();

      let target = null;
      if (event.keyCode == customize.KEYCODES.LEFT) {
        target = customize.getNextTile(
            document.documentElement.classList.contains('rtl') ? 1 : -1, 0,
            event.currentTarget);
      } else if (event.keyCode == customize.KEYCODES.UP) {
        target = customize.getNextTile(0, -1, event.currentTarget);
      } else if (event.keyCode == customize.KEYCODES.RIGHT) {
        target = customize.getNextTile(
            document.documentElement.classList.contains('rtl') ? -1 : 1, 0,
            event.currentTarget);
      } else if (event.keyCode == customize.KEYCODES.DOWN) {
        target = customize.getNextTile(0, 1, event.currentTarget);
      }
      if (target) {
        target.focus();
      } else {
        event.currentTarget.focus();
      }
    }
  };

  const preLoadTiles = [];
  const postLoadTiles = [];

  for (let i = 0; i < collImg.length; ++i) {
    const dataset = {};

    dataset.attributionLine1 =
        (collImg[i].attributions[0] !== undefined ? collImg[i].attributions[0] :
                                                    '');
    dataset.attributionLine2 =
        (collImg[i].attributions[1] !== undefined ? collImg[i].attributions[1] :
                                                    '');
    dataset.attributionActionUrl = collImg[i].attributionActionUrl;
    dataset.url = collImg[i].imageUrl;
    dataset.tileIndex = i;


    let tileId = 'img_tile_' + i;
    if (configData.richerPicker) {
      tileId = 'coll_' + collIndex + '_' + tileId;
    }
    const tile = customize.createTileThumbnail(
        tileId, collImg[i].imageUrl, dataset, tileOnClickInteraction,
        tileOnKeyDownInteraction);

    tile.setAttribute('aria-label', collImg[i].attributions[0]);

    // Load the first |ROWS_TO_PRELOAD| rows of tiles.
    if (i < firstNTile) {
      preLoadTiles.push(tile);
    } else {
      postLoadTiles.push(tile);
    }

    const tileBackground = document.createElement('div');
    tileBackground.classList.add(customize.CLASSES.COLLECTION_TILE_BG);
    tileBackground.appendChild(tile);
    tileContainer.appendChild(tileBackground);
  }
  let tileGetsLoaded = 0;
  for (const tile of preLoadTiles) {
    customize.loadTile(tile, collImg, () => {
      // After the preloaded tiles finish loading, the rest of the tiles start
      // loading.
      if (++tileGetsLoaded === preLoadTiles.length) {
        postLoadTiles.forEach(
            (tile) => customize.loadTile(tile, collImg, null));
      }
    });
  }

  // If an image tile was previously selected re-select it now.
  if (customize.selectedOptions.backgroundData) {
    const selected = $(customize.selectedOptions.backgroundData.id);
    if (selected) {
      customize.richerPicker_selectBackgroundTile(selected);
    }
  }

  if (configData.richerPicker) {
    $(customize.IDS.BACKGROUNDS_IMAGE_MENU).focus();
  } else {
    $(customize.IDS.TILES).focus();
  }
};

/**
 * Add background image src to the tile and add animation for the tile once it
 * successfully loaded.
 * @param {!Object} tile the tile that needs to be loaded.
 * @param {!Object} imageData the source imageData.
 * @param {?Function} countLoad If not null, called after the tile finishes
 *     loading.
 */
customize.loadTile = function(tile, imageData, countLoad) {
  tile.style.backgroundImage =
      'url(' + imageData[tile.dataset.tileIndex].thumbnailImageUrl + ')';
  customize.fadeInImageTile(
      tile, imageData[tile.dataset.tileIndex].thumbnailImageUrl, countLoad);
};

/**
 * Fade in effect for both collection and image tile. Once the image
 * successfully loads, we can assume the background image with the same source
 * has also loaded. Then, we set opacity for the tile to start the animation.
 * @param {!Object} tile The tile to add the fade in animation to.
 * @param {string} imageUrl the image url for the tile
 * @param {?Function} countLoad If not null, called after the tile finishes
 *     loading.
 */
customize.fadeInImageTile = function(tile, imageUrl, countLoad) {
  const image = new Image();
  image.onload = () => {
    tile.style.opacity = '1';
    if (countLoad) {
      countLoad();
    }
  };
  image.src = imageUrl;
};

/**
 * Load the NTPBackgroundCollections script. It'll create a global
 * variable name "coll" which is a dict of background collections data.
 */
customize.loadChromeBackgrounds = function() {
  const collElement = $('ntp-collection-loader');
  if (collElement) {
    collElement.parentNode.removeChild(collElement);
  }
  const collScript = document.createElement('script');
  collScript.id = 'ntp-collection-loader';
  collScript.src = 'chrome-search://local-ntp/ntp-background-collections.js?' +
      'collection_type=background';
  collScript.onload = function() {
    if (configData.richerPicker) {
      customize.showCollectionSelectionDialog();
    }
  };
  document.body.appendChild(collScript);
};

/**
 * Close dialog when an image is selected via the file picker.
 */
customize.closeCustomizationDialog = function() {
  if (configData.richerPicker) {
    $(customize.IDS.CUSTOMIZATION_MENU).close();
  } else {
    $(customize.IDS.EDIT_BG_DIALOG).close();
  }
};

/**
 * Get the next visible option. There are times when various combinations of
 * options are hidden.
 * @param {number} current_index Index of the option the key press occurred on.
 * @param {number} deltaY Direction to search in, -1 for up, 1 for down.
 */
customize.getNextOption = function(current_index, deltaY) {
  // Create array corresponding to the menu. Important that this is in the same
  // order as the MENU_ENTRIES enum, so we can index into it.
  const entries = [];
  entries.push($(customize.IDS.DEFAULT_WALLPAPERS));
  entries.push($(customize.IDS.UPLOAD_IMAGE));
  entries.push($(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT));
  entries.push($(customize.IDS.RESTORE_DEFAULT));

  let idx = current_index;
  do {
    idx = idx + deltaY;
    if (idx === -1) {
      idx = 3;
    }
    if (idx === 4) {
      idx = 0;
    }
  } while (
      idx !== current_index &&
      (entries[idx].hidden ||
       entries[idx].classList.contains(customize.CLASSES.OPTION_DISABLED)));
  return entries[idx];
};

/**
 * Hide custom background options based on the network state
 * @param {boolean} online The current state of the network
 */
customize.networkStateChanged = function(online) {
  $(customize.IDS.DEFAULT_WALLPAPERS).hidden = !online;
};

/**
 * Open the customization menu and set it to the default submenu (Background).
 */
customize.richerPicker_openCustomizationMenu = function() {
  customize.richerPicker_showSubmenu(
      $(customize.IDS.BACKGROUNDS_BUTTON), $(customize.IDS.BACKGROUNDS_MENU));

  customize.richerPicker_preselectShortcutOptions();
  customize.loadChromeBackgrounds();
  customize.loadColorsMenu();
  if (!$(customize.IDS.CUSTOMIZATION_MENU).open) {
    $(customize.IDS.CUSTOMIZATION_MENU).showModal();
  }
};

/**
 * Reset the selected options in the customization menu.
 */
customize.richerPicker_resetSelectedOptions = function() {
  // Reset background selection.
  customize.richerPicker_deselectBackgroundTile(
      customize.selectedOptions.background);
  customize.selectedOptions.background = null;
  customize.selectedOptions.backgroundData = null;

  // Reset color selection.
  customize.richerPicker_removeSelectedState(customize.selectedOptions.color);
  customize.selectedOptions.color = null;

  customize.richerPicker_preselectShortcutOptions();
};

/**
 * Preselect the shortcut type and visibility to reflect the current state on
 * the page.
 */
customize.richerPicker_preselectShortcutOptions = function() {
  const shortcutType = chrome.embeddedSearch.newTabPage.isUsingMostVisited ?
      $(customize.IDS.SHORTCUTS_OPTION_MOST_VISITED) :
      $(customize.IDS.SHORTCUTS_OPTION_CUSTOM_LINKS);
  const shortcutsAreHidden =
      !chrome.embeddedSearch.newTabPage.areShortcutsVisible;
  customize.preselectedShortcutOptions.shortcutType = shortcutType;
  customize.preselectedShortcutOptions.shortcutsAreHidden = shortcutsAreHidden;
  customize.richerPicker_selectShortcutType(shortcutType);
  customize.richerPicker_toggleShortcutHide(shortcutsAreHidden);
};

/**
 * Resets the customization menu.
 */
customize.richerPicker_resetCustomizationMenu = function() {
  customize.richerPicker_resetSelectedOptions();
  customize.richerPicker_resetImageMenu();
  customize.richerPicker_hideOpenSubmenu();
};

/**
 * Close and reset the customization menu.
 */
customize.richerPicker_closeCustomizationMenu = function() {
  $(customize.IDS.BACKGROUNDS_MENU).scrollTop = 0;
  $(customize.IDS.CUSTOMIZATION_MENU).close();
  customize.richerPicker_resetCustomizationMenu();

  customize.richerPicker_unpreviewImage();
};

/**
 * Cancel customization, revert any changes, and close the richer picker.
 */
customize.richerPicker_cancelCustomization = function() {
  // Cancel any color changes.
  if (customize.selectedOptions.color) {
    customize.cancelColor();
  }

  customize.richerPicker_closeCustomizationMenu();
};

/**
 * Apply the currently selected customization options and close the richer
 * picker.
 */
customize.richerPicker_applyCustomization = function() {
  if (customize.selectedOptions.backgroundData) {
    customize.setBackground(
        customize.selectedOptions.backgroundData.url,
        customize.selectedOptions.backgroundData.attr1,
        customize.selectedOptions.backgroundData.attr2,
        customize.selectedOptions.backgroundData.attrUrl);
  }
  if (customize.richerPicker_isShortcutOptionSelected()) {
    customize.richerPicker_setShortcutOptions();
  }
  if (customize.selectedOptions.color) {
    customize.confirmColor();
  }
  customize.richerPicker_closeCustomizationMenu();
};

/**
 * Initialize the settings menu, custom backgrounds dialogs, and custom
 * links menu items. Set the text and event handlers for the various
 * elements.
 * @param {!Function} showErrorNotification Called when the error notification
 *     should be displayed.
 * @param {!Function} hideCustomLinkNotification Called when the custom link
 *     notification should be hidden.
 */
customize.init = function(showErrorNotification, hideCustomLinkNotification) {
  ntpApiHandle = window.chrome.embeddedSearch.newTabPage;
  const editDialog = $(customize.IDS.EDIT_BG_DIALOG);
  const menu = $(customize.IDS.MENU);

  $(customize.IDS.OPTIONS_TITLE).textContent =
      configData.translatedStrings.customizeThisPage;

  if (configData.richerPicker) {
    // Store the main menu title so it can be restored if needed.
    customize.richerPicker_defaultTitle =
        $(customize.IDS.MENU_TITLE).textContent;
  }

  $(customize.IDS.EDIT_BG)
      .setAttribute(
          'aria-label', configData.translatedStrings.customizeThisPage);

  $(customize.IDS.EDIT_BG)
      .setAttribute('title', configData.translatedStrings.customizeThisPage);

  // Selecting a local image for the background should close the picker.
  if (configData.richerPicker) {
    ntpApiHandle.onlocalbackgroundselected = () => {
      customize.richerPicker_deselectBackgroundTile(
          customize.selectedOptions.background);
      customize.richerPicker_applyCustomization();
    };
  }

  // Edit gear icon interaction events.
  const editBackgroundInteraction = function() {
    if (configData.richerPicker) {
      customize.richerPicker_openCustomizationMenu();
    } else {
      editDialog.showModal();
    }
  };
  $(customize.IDS.EDIT_BG).onclick = function(event) {
    editDialog.classList.add(customize.CLASSES.MOUSE_NAV);
    editBackgroundInteraction();
  };

  $(customize.IDS.MENU_CANCEL).onclick = function(event) {
    customize.richerPicker_cancelCustomization();
  };


  // Find the first menu option that is not hidden or disabled.
  const findFirstMenuOption = () => {
    const editMenu = $(customize.IDS.EDIT_BG_MENU);
    for (let i = 1; i < editMenu.children.length; i++) {
      const option = editMenu.children[i];
      if (option.classList.contains(customize.CLASSES.OPTION) &&
          !option.hidden &&
          !option.classList.contains(customize.CLASSES.OPTION_DISABLED)) {
        option.focus();
        return;
      }
    }
  };

  $(customize.IDS.EDIT_BG).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      // no default behavior for ENTER
      event.preventDefault();
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
      editBackgroundInteraction();
      findFirstMenuOption();
    }
  };

  // Interactions to close the customization option dialog.
  const editDialogInteraction = function() {
    editDialog.close();
  };
  editDialog.onclick = function(event) {
    editDialog.classList.add(customize.CLASSES.MOUSE_NAV);
    if (event.target === editDialog) {
      editDialogInteraction();
    }
  };
  editDialog.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ESC) {
      editDialogInteraction();
    } else if (
        editDialog.classList.contains(customize.CLASSES.MOUSE_NAV) &&
        (event.keyCode === customize.KEYCODES.TAB ||
         event.keyCode === customize.KEYCODES.UP ||
         event.keyCode === customize.KEYCODES.DOWN)) {
      // When using tab in mouse navigation mode, select the first option
      // available.
      event.preventDefault();
      findFirstMenuOption();
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
    } else if (event.keyCode === customize.KEYCODES.TAB) {
      // If keyboard navigation is attempted, remove mouse-only mode.
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
    } else if (customize.arrowKeys.includes(event.keyCode)) {
      event.preventDefault();
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
    }
  };

  customize.initCustomLinksItems(hideCustomLinkNotification);
  customize.initCustomBackgrounds(showErrorNotification);
};

/**
 * Initialize custom link items in the settings menu dialog. Set the text
 * and event handlers for the various elements.
 * @param {!Function} hideCustomLinkNotification Called when the custom link
 *     notification should be hidden.
 */
customize.initCustomLinksItems = function(hideCustomLinkNotification) {
  customize.hideCustomLinkNotification = hideCustomLinkNotification;

  const editDialog = $(customize.IDS.EDIT_BG_DIALOG);
  const menu = $(customize.IDS.MENU);

  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT_TEXT).textContent =
      configData.translatedStrings.restoreDefaultLinks;

  // Interactions with the "Restore default shortcuts" option.
  const customLinksRestoreDefaultInteraction = function() {
    editDialog.close();
    customize.hideCustomLinkNotification();
    window.chrome.embeddedSearch.newTabPage.resetCustomLinks();
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED);
  };
  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).onclick = () => {
    if (!$(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT)
             .classList.contains(customize.CLASSES.OPTION_DISABLED)) {
      customLinksRestoreDefaultInteraction();
    }
  };
  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      customLinksRestoreDefaultInteraction();
    } else if (event.keyCode === customize.KEYCODES.UP) {
      // Handle arrow key navigation.
      event.preventDefault();
      customize
          .getNextOption(
              customize.MENU_ENTRIES.CUSTOM_LINKS_RESTORE_DEFAULT, -1)
          .focus();
    } else if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize
          .getNextOption(customize.MENU_ENTRIES.CUSTOM_LINKS_RESTORE_DEFAULT, 1)
          .focus();
    }
  };
};

/**
 * Initialize the settings menu and custom backgrounds dialogs. Set the
 * text and event handlers for the various elements.
 * @param {!Function} showErrorNotification Called when the error notification
 *     should be displayed.
 */
customize.initCustomBackgrounds = function(showErrorNotification) {
  customize.showErrorNotification = showErrorNotification;

  const editDialog = $(customize.IDS.EDIT_BG_DIALOG);
  const menu = $(customize.IDS.MENU);

  $(customize.IDS.DEFAULT_WALLPAPERS_TEXT).textContent =
      configData.translatedStrings.defaultWallpapers;
  $(customize.IDS.UPLOAD_IMAGE_TEXT).textContent =
      configData.translatedStrings.uploadImage;
  $(customize.IDS.RESTORE_DEFAULT_TEXT).textContent =
      configData.translatedStrings.restoreDefaultBackground;
  $(customize.IDS.DONE).textContent =
      configData.translatedStrings.selectionDone;
  $(customize.IDS.CANCEL).textContent =
      configData.translatedStrings.selectionCancel;

  window.addEventListener('online', function(event) {
    customize.networkStateChanged(true);
  });

  window.addEventListener('offline', function(event) {
    customize.networkStateChanged(false);
  });

  if (!window.navigator.onLine) {
    customize.networkStateChanged(false);
  }

  $(customize.IDS.BACK_CIRCLE)
      .setAttribute('aria-label', configData.translatedStrings.backLabel);
  $(customize.IDS.CANCEL)
      .setAttribute('aria-label', configData.translatedStrings.selectionCancel);
  $(customize.IDS.DONE)
      .setAttribute('aria-label', configData.translatedStrings.selectionDone);

  $(customize.IDS.DONE).disabled = true;

  // Interactions with the "Upload an image" option.
  const uploadImageInteraction = function() {
    window.chrome.embeddedSearch.newTabPage.selectLocalBackgroundImage();
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED);
  };

  $(customize.IDS.UPLOAD_IMAGE).onclick = (event) => {
    if (!$(customize.IDS.UPLOAD_IMAGE)
             .classList.contains(customize.CLASSES.OPTION_DISABLED)) {
      uploadImageInteraction();
    }
  };
  $(customize.IDS.UPLOAD_IMAGE).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      uploadImageInteraction();
    }

    // Handle arrow key navigation.
    if (event.keyCode === customize.KEYCODES.UP) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.UPLOAD_IMAGE, -1).focus();
    }
    if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.UPLOAD_IMAGE, 1).focus();
    }
  };

  // Interactions with the "Restore default background" option.
  const restoreDefaultInteraction = function() {
    editDialog.close();
    customize.clearAttribution();
    window.chrome.embeddedSearch.newTabPage.setBackgroundURL('');
  };
  $(customize.IDS.RESTORE_DEFAULT).onclick = (event) => {
    if (!$(customize.IDS.RESTORE_DEFAULT)
             .classList.contains(customize.CLASSES.OPTION_DISABLED)) {
      restoreDefaultInteraction();
    }
  };
  $(customize.IDS.RESTORE_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      restoreDefaultInteraction();
    }

    // Handle arrow key navigation.
    if (event.keyCode === customize.KEYCODES.UP) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.RESTORE_DEFAULT, -1)
          .focus();
    }
    if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.RESTORE_DEFAULT, 1)
          .focus();
    }
  };

  // Interactions with the "Chrome backgrounds" option.
  const defaultWallpapersInteraction = function(event) {
    customize.loadChromeBackgrounds();
    $('ntp-collection-loader').onload = function() {
      editDialog.close();
      if (typeof coll != 'undefined' && coll.length > 0) {
        customize.showCollectionSelectionDialog();
      } else {
        customize.handleError(collErrors);
      }
    };
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED);
  };
  $(customize.IDS.DEFAULT_WALLPAPERS).onclick = function(event) {
    $(customize.IDS.MENU).classList.add(customize.CLASSES.MOUSE_NAV);
    defaultWallpapersInteraction(event);
  };
  $(customize.IDS.DEFAULT_WALLPAPERS).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      $(customize.IDS.MENU).classList.remove(customize.CLASSES.MOUSE_NAV);
      defaultWallpapersInteraction(event);
    }

    // Handle arrow key navigation.
    if (event.keyCode === customize.KEYCODES.UP) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.CHROME_BACKGROUNDS, -1)
          .focus();
    }
    if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.CHROME_BACKGROUNDS, 1)
          .focus();
    }
  };

  // Escape and Backspace handling for the background picker dialog.
  menu.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.SPACE) {
      $(customize.IDS.TILES).scrollTop += $(customize.IDS.TILES).offsetHeight;
      event.stopPropagation();
      event.preventDefault();
    }
    if (event.keyCode === customize.KEYCODES.ESC ||
        event.keyCode === customize.KEYCODES.BACKSPACE) {
      event.preventDefault();
      event.stopPropagation();
      if (menu.classList.contains(customize.CLASSES.COLLECTION_DIALOG)) {
        menu.close();
        customize.resetSelectionDialog();
      } else {
        customize.resetSelectionDialog();
        customize.showCollectionSelectionDialog();
      }
    }

    // If keyboard navigation is attempted, remove mouse-only mode.
    if (event.keyCode === customize.KEYCODES.TAB ||
        event.keyCode === customize.KEYCODES.LEFT ||
        event.keyCode === customize.KEYCODES.UP ||
        event.keyCode === customize.KEYCODES.RIGHT ||
        event.keyCode === customize.KEYCODES.DOWN) {
      menu.classList.remove(customize.CLASSES.MOUSE_NAV);
    }
  };

  // Interactions with the back arrow on the image selection dialog.
  const backInteraction = function(event) {
    if (configData.richerPicker) {
      customize.richerPicker_resetImageMenu();
    }
    customize.resetSelectionDialog();
    customize.showCollectionSelectionDialog();
  };
  $(customize.IDS.BACK_CIRCLE).onclick = backInteraction;
  $(customize.IDS.MENU_BACK_CIRCLE).onclick = backInteraction;
  $(customize.IDS.BACK_CIRCLE).onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      backInteraction(event);
    }
  };
  $(customize.IDS.MENU_BACK_CIRCLE).onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      backInteraction(event);
    }
  };
  // Pressing Spacebar on the back arrow shouldn't scroll the dialog.
  $(customize.IDS.BACK_CIRCLE).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.SPACE) {
      event.stopPropagation();
    }
  };

  // Interactions with the cancel button on the background picker dialog.
  $(customize.IDS.CANCEL).onclick = function(event) {
    customize.closeCollectionDialog(menu);
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL);
  };
  $(customize.IDS.CANCEL).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      customize.closeCollectionDialog(menu);
      ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                                .NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL);
    }
  };

  // Interactions with the done button on the background picker dialog.
  const doneInteraction = function(event) {
    const done = configData.richerPicker ? $(customize.IDS.MENU_DONE) :
                                           $(customize.IDS.DONE);
    if (done.disabled) {
      return;
    }
    if (configData.richerPicker) {
      customize.richerPicker_applyCustomization();
    } else if (customize.selectedOptions.background) {
      // Also closes the customization menu.
      customize.setBackground(
          customize.selectedOptions.background.dataset.url,
          customize.selectedOptions.background.dataset.attributionLine1,
          customize.selectedOptions.background.dataset.attributionLine2,
          customize.selectedOptions.background.dataset.attributionActionUrl);
    }
  };
  $(customize.IDS.DONE).onclick = doneInteraction;
  $(customize.IDS.MENU_DONE).onclick = doneInteraction;
  $(customize.IDS.DONE).onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      doneInteraction(event);
    }
  };

  // On any arrow key event in the tiles area, focus the first tile.
  $(customize.IDS.TILES).onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      event.preventDefault();
      if ($(customize.IDS.MENU)
              .classList.contains(customize.CLASSES.COLLECTION_DIALOG)) {
        $('coll_tile_0').focus();
      } else {
        document.querySelector('[id$="img_tile_0"]').focus();
      }
    }
  };

  $(customize.IDS.BACKGROUNDS_MENU).onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      $(customize.IDS.BACKGROUNDS_UPLOAD_WRAPPER).focus();
    }
  };

  $(customize.IDS.BACKGROUNDS_IMAGE_MENU).onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      document.querySelector('[id$="img_tile_0"]').focus();
    }
  };

  $(customize.IDS.BACKGROUNDS_UPLOAD).onclick = uploadImageInteraction;
  $(customize.IDS.BACKGROUNDS_UPLOAD).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      uploadImageInteraction();
    }
  };

  $(customize.IDS.BACKGROUNDS_DEFAULT).onclick = function(event) {
    const tile = $(customize.IDS.BACKGROUNDS_DEFAULT_ICON);
    tile.dataset.url = '';
    tile.dataset.attributionLine1 = '';
    tile.dataset.attributionLine2 = '';
    tile.dataset.attributionActionUrl = '';
    if ($(customize.IDS.BACKGROUNDS_DEFAULT)
            .classList.contains(customize.CLASSES.SELECTED)) {
      customize.richerPicker_deselectBackgroundTile(tile);
    } else {
      customize.richerPicker_selectBackgroundTile(tile);
    }
  };
  $(customize.IDS.BACKGROUNDS_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      $(customize.IDS.BACKGROUNDS_DEFAULT).onclick(event);
    }
  };

  const richerPicker = $(customize.IDS.CUSTOMIZATION_MENU);
  richerPicker.onclick = function(event) {
    richerPicker.classList.add(customize.CLASSES.MOUSE_NAV);
  };
  richerPicker.onkeydown = function(event) {
    richerPicker.classList.remove(customize.CLASSES.MOUSE_NAV);

    if (event.keyCode === customize.KEYCODES.BACKSPACE &&
        customize.richerPicker_selectedSubmenu.menu.id ===
            customize.IDS.BACKGROUNDS_IMAGE_MENU) {
      backInteraction(event);
    } else if (
        event.keyCode === customize.KEYCODES.ESC ||
        event.keyCode === customize.KEYCODES.BACKSPACE) {
      customize.richerPicker_cancelCustomization();
    }
  };

  const richerPickerOpenBackgrounds = function() {
    // Open the previously open Background submenu, if applicable.
    customize.richerPicker_showSubmenu(
        $(customize.IDS.BACKGROUNDS_BUTTON),
        $(customize.richerPicker_openBackgroundSubmenu.menuId),
        customize.richerPicker_openBackgroundSubmenu.title);
  };

  $(customize.IDS.BACKGROUNDS_BUTTON).onclick = richerPickerOpenBackgrounds;
  $(customize.IDS.BACKGROUNDS_BUTTON).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      richerPickerOpenBackgrounds();
    }
  };

  const clOption = $(customize.IDS.SHORTCUTS_OPTION_CUSTOM_LINKS);
  clOption.onclick = function() {
    customize.richerPicker_selectShortcutType(clOption);
  };
  clOption.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      customize.richerPicker_selectShortcutType(clOption);
    }
  };

  const mvOption = $(customize.IDS.SHORTCUTS_OPTION_MOST_VISITED);
  mvOption.onclick = function() {
    customize.richerPicker_selectShortcutType(mvOption);
  };
  mvOption.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      customize.richerPicker_selectShortcutType(mvOption);
    }
  };

  const hideToggle = $(customize.IDS.SHORTCUTS_HIDE_TOGGLE);
  hideToggle.onchange = function(event) {
    customize.richerPicker_toggleShortcutHide(hideToggle.checked);
  };
  hideToggle.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      hideToggle.onchange(event);
    }
  };
  hideToggle.onclick = function(event) {
    // Enter and space fire the 'onclick' event (which will remove special
    // keyboard navigation styling) unless propagation is stopped.
    event.stopPropagation();
  };

  const richerPickerOpenShortcuts = function() {
    customize.richerPicker_showSubmenu(
        $(customize.IDS.SHORTCUTS_BUTTON), $(customize.IDS.SHORTCUTS_MENU));
  };

  $(customize.IDS.SHORTCUTS_BUTTON).onclick = richerPickerOpenShortcuts;
  $(customize.IDS.SHORTCUTS_BUTTON).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      richerPickerOpenShortcuts();
    }
  };

  $(customize.IDS.COLORS_BUTTON).onclick = function() {
    customize.richerPicker_showSubmenu(
        $(customize.IDS.COLORS_BUTTON), $(customize.IDS.COLORS_MENU));
    ntpApiHandle.getColorsInfo();
  };
};

customize.handleError = function(errors) {
  const unavailableString = configData.translatedStrings.backgroundsUnavailable;

  if (errors != 'undefined') {
    // Network errors.
    if (errors.net_error) {
      if (errors.net_error_no != 0) {
        const onClick = () => {
          window.open(
              'https://chrome://network-error/' + errors.net_error_no,
              '_blank');
        };
        customize.showErrorNotification(
            configData.translatedStrings.connectionError,
            configData.translatedStrings.moreInfo, onClick);
      } else {
        customize.showErrorNotification(
            configData.translatedStrings.connectionErrorNoPeriod);
      }
    } else if (errors.service_error) {  // Service errors.
      customize.showErrorNotification(unavailableString);
    }
    return;
  }

  // Generic error when we can't tell what went wrong.
  customize.showErrorNotification(unavailableString);
};

/**
 * Handles color selection. Apply styling to the selected color in the richer
 * picker and enable the done button.
 * @param {?Element} tile The selected color tile.
 */
customize.updateColorsMenuTileSelection = function(tile) {
  if (!tile) {
    return;
  }
  // Clear the previous selection, if any.
  if (customize.selectedOptions.color) {
    customize.richerPicker_removeSelectedState(customize.selectedOptions.color);
  }
  customize.selectedOptions.color = tile;
  customize.richerPicker_applySelectedState(tile);
  customize.richerPicker_maybeToggleDone();
};

/**
 * Called when a color tile is clicked. Applies the color, and the selected
 * style on the tile.
 * @param {Event} event The event attributes for the interaction.
 */
customize.colorTileInteraction = function(event) {
  customize.updateColorsMenuTileSelection(
      /** @type HTMLElement */ (event.target));
  ntpApiHandle.applyAutogeneratedTheme(event.target.dataset.color.split(','));
};

/**
 * Called when the default theme tile is clicked. Applies the default theme, and
 * the selected style on the tile.
 * @param {Event} event The event attributes for the interaction.
 */
customize.defaultThemeTileInteraction = function(event) {
  customize.updateColorsMenuTileSelection(
      /** @type HTMLElement */ (event.target));
  ntpApiHandle.applyDefaultTheme();
};

/**
 * Loads Colors menu elements.
 */
customize.loadColorsMenu = function() {
  if (customize.colorsMenuLoaded) {
    return;
  }

  customize.updateWebstoreThemeInfo();

  const colorsColl = ntpApiHandle.getColorsInfo();
  for (let i = 0; i < colorsColl.length; ++i) {
    const id = 'color_' + i;
    const imageUrl = colorsColl[i].icon;
    const dataset = {'color': colorsColl[i].color};

    const tile = customize.createTileWithoutTitle(
        id, imageUrl, dataset, customize.colorTileInteraction,
        customize.colorTileInteraction);
    $(customize.IDS.COLORS_MENU).appendChild(tile);
  }

  // Configure the default tile.
  $(customize.IDS.COLORS_DEFAULT_ICON).onclick =
      customize.defaultThemeTileInteraction;

  customize.colorsMenuLoaded = true;
};

/**
 * Update webstore theme info for Colors menu.
 */
customize.updateWebstoreThemeInfo = function() {
  const themeInfo = ntpApiHandle.themeBackgroundInfo;
  if (themeInfo.themeId && themeInfo.themeName) {
    $(customize.IDS.COLORS_THEME).classList.add(customize.CLASSES.VISIBLE);
    $(customize.IDS.COLORS_THEME_NAME).innerHTML = themeInfo.themeName;
    $(customize.IDS.COLORS_THEME_WEBSTORE_LINK).href =
        'https://chrome.google.com/webstore/detail/' + themeInfo.themeId;
    $(customize.IDS.COLORS_THEME_UNINSTALL).onclick =
        ntpApiHandle.useDefaultTheme;
  } else {
    $(customize.IDS.COLORS_THEME).classList.remove(customize.CLASSES.VISIBLE);
  }
};

/**
 * Permanently applies the color changes. Called when the done button is
 * pressed.
 */
customize.confirmColor = function() {
  ntpApiHandle.confirmThemeChanges();
};

/**
 * Reverts the applied (but not confirmed) color changes. Called when the cancel
 * button is pressed.
 */
customize.cancelColor = function() {
  ntpApiHandle.revertThemeChanges();
};
