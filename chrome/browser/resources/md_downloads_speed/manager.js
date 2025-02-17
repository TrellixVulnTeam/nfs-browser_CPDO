/**
 * Copyright (c) 2016-2018 CPU and Fundamental Software Research Center, CAS
 *
 * This software is published by the license of CPU-OS Licence, you can use and
 * distribute this software under this License. See CPU-OS License for more detail.
 *
 * You should have received a copy of CPU-OS License. If not, please contact us
 * by email <support_os@cpu-os.ac.cn>
 *
**/

cr.define('downloads', function() {
  var Manager = Polymer({
    is: 'downloads-manager',

    properties: {
      hasDownloads_: {
        observer: 'hasDownloadsChanged_',
        type: Boolean,
      },

      items_: {
        type: Array,
        value: function() { return []; },
      },
    },

    hostAttributes: {
      loading: true,
    },

    listeners: {
      'downloads-list.scroll': 'onListScroll_',
    },

    observers: [
      'itemsChanged_(items_.*)',
    ],

    /** @private */
    clearAll_: function() {
      this.set('items_', []);
    },

    /** @private */
    onNewItemTap_: function() {
      downloads.ActionService.getInstance().newItem();
    },

    /** @private */
    onClearAllTap_: function() {
      downloads.ActionService.getInstance().clearAll();
    },

    /** @private */
    onOpenSettingTap_: function() {
      downloads.ActionService.getInstance().openSetting();
    },

    /** @private */
    onRightMenu_: function(x, y) {
      var scrollTop = this.$['downloads-list'].scrollTop;
      var index = parseInt((y+scrollTop)/70);
      var select = this.items_[index];
      downloads.ActionService.getInstance().rightMenu(select.id, x, y);
    },

    /** @private */
    hasDownloadsChanged_: function() {
      //if (loadTimeData.getBoolean('allowDeletingHistory'))
      //  this.$.toolbar.downloadsShowing = this.hasDownloads_;

      if (this.hasDownloads_) {
        this.$['downloads-list'].fire('iron-resize');
      } else {
        //var isSearching = downloads.ActionService.getInstance().isSearching();
        //var messageToShow = isSearching ? 'noSearchResults' : 'noDownloads';
        //this.$['no-downloads'].querySelector('span').textContent =
        //    loadTimeData.getString(messageToShow);
      }
    },

    /**
     * @param {number} index
     * @param {!Array<!downloads.Data>} list
     * @private
     */
    insertItems_: function(index, list) {
      this.splice.apply(this, ['items_', index, 0].concat(list));
      this.updateHideDates_(index, index + list.length);
      this.removeAttribute('loading');
    },

    /** @private */
    itemsChanged_: function() {
      this.hasDownloads_ = this.items_.length > 0;
    },

    /**
     * @param {Event} e
     * @private
     */
    onCanExecute_: function(e) {
      e = /** @type {cr.ui.CanExecuteEvent} */(e);
      switch (e.command.id) {
        case 'undo-command':
          //e.canExecute = this.$.toolbar.canUndo();
          break;
        case 'clear-all-command':
          //e.canExecute = this.$.toolbar.canClearAll();
          break;
        case 'find-command':
          e.canExecute = true;
          break;
      }
    },

    /**
     * @param {Event} e
     * @private
     */
    onCommand_: function(e) {
      if (e.command.id == 'clear-all-command')
        downloads.ActionService.getInstance().clearAll();
      else if (e.command.id == 'undo-command')
        downloads.ActionService.getInstance().undo();
      //else if (e.command.id == 'find-command')
      //  this.$.toolbar.onFindCommand();
    },

    /** @private */
    onListScroll_: function() {
      var list = this.$['downloads-list'];
      if (list.scrollHeight - list.scrollTop - list.offsetHeight <= 100) {
        // Approaching the end of the scrollback. Attempt to load more items.
        downloads.ActionService.getInstance().loadMore();
      }
    },

    /** @private */
    onLoad_: function() {
      cr.ui.decorate('command', cr.ui.Command);
      document.addEventListener('canExecute', this.onCanExecute_.bind(this));
      document.addEventListener('command', this.onCommand_.bind(this));

      downloads.ActionService.getInstance().loadMore();
    },

    /**
     * @param {number} index
     * @private
     */
    removeItem_: function(index) {
      this.splice('items_', index, 1);
      this.updateHideDates_(index, index);
      this.onListScroll_();
    },

    /**
     * @param {number} start
     * @param {number} end
     * @private
     */
    updateHideDates_: function(start, end) {
      for (var i = start; i <= end; ++i) {
        var current = this.items_[i];
        if (!current)
          continue;
        var prev = this.items_[i - 1];
        current.hideDate = !!prev && prev.date_string == current.date_string;
      }
    },

    /**
     * @param {number} index
     * @param {!downloads.Data} data
     * @private
     */
    updateItem_: function(index, data) {
      this.set('items_.' + index, data);
      this.updateHideDates_(index, index);
      var list = /** @type {!IronListElement} */(this.$['downloads-list']);
      list.updateSizeForItem(index);
    },
  });

  Manager.clearAll = function() {
    Manager.get().clearAll_();
  };

  /** @return {!downloads.Manager} */
  Manager.get = function() {
    return /** @type {!downloads.Manager} */(
        queryRequiredElement('downloads-manager'));
  };

  Manager.insertItems = function(index, list) {
    Manager.get().insertItems_(index, list);
  };

  Manager.onLoad = function() {
    Manager.get().onLoad_();
  };

  Manager.removeItem = function(index) {
    Manager.get().removeItem_(index);
  };

  Manager.updateItem = function(index, data) {
    Manager.get().updateItem_(index, data);
  };

  window.onmouseup = function() {
    if (event.button == 2) {
      if (Manager.get().hasDownloads_) {        
        Manager.get().onRightMenu_(event.pageX, event.pageY);
      }
    }
  };

  return {Manager: Manager};
});

window.oncontextmenu  = function() {
	return false;
}

window.addEventListener('load', downloads.Manager.onLoad);