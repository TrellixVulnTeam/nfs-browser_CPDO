// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline login implementation.
 */

/**
 * Initialize the offline page.
 * @param {Object} params Intialization params passed from parent page.
 */
function load(params) {
  // Setup localized strings.
  $('sign-in-title').textContent = decodeURIComponent(params['stringSignIn']);
  $('email-label').textContent = decodeURIComponent(params['stringEmail']);
  $('password-label').textContent =
      decodeURIComponent(params['stringPassword']);
  $('submit-button').value = decodeURIComponent(params['stringSignIn']);
  $('empty-email-alert').textContent =
      decodeURIComponent(params['stringEmptyEmail']);
  $('empty-password-alert').textContent =
      decodeURIComponent(params['stringEmptyPassword']);
  $('errormsg-alert').textContent = decodeURIComponent(params['stringError']);

  // Setup actions.
  var form = $('offline-login-form');
  form.addEventListener('submit', function(e) {
    // Clear all previous errors.
    form.email.classList.remove('field-error');
    form.password.classList.remove('field-error');
    form.password.classList.remove('form-error');

    if (form.email.value == '') {
      form.email.classList.add('field-error');
      form.email.focus();
    } else if (form.password.value == '') {
      form.password.classList.add('field-error');
      form.password.focus();
    } else {
      var msg = {
        'method': 'offlineLogin',
        'email': form.email.value,
        'password': form.password.value
      };
      window.parent.postMessage(msg, 'nfsbrowser://oobe/');
    }
    e.preventDefault();
  });

  var email = params['email'];
  if (email) {
    // Email is present, which means that unsuccessful login attempt has been
    // made. Try to mimic Gaia's behaviour.
    form.email.value = email;
    form.password.classList.add('form-error');
    form.password.focus();
  } else {
    form.email.focus();
  }
  window.parent.postMessage({'method': 'loginUILoaded'}, 'nfsbrowser://oobe/');
}

/**
 * Handles initialization message from parent page.
 * @param {MessageEvent} e
 */
function handleInitializeMessage(e) {
  var ALLOWED_PARENT_ORIGINS = [
    'nfsbrowser://oobe',
    'nfsbrowser://chrome-signin'
  ];

  if (ALLOWED_PARENT_ORIGINS.indexOf(e.origin) == -1)
    return;

  window.removeEventListener('message', handleInitializeMessage);

  var params = e.data;
  params.parentPage = e.origin;
  load(params);
}

document.addEventListener('DOMContentLoaded', function() {
  window.addEventListener('message', handleInitializeMessage);
  window.parent.postMessage({'method': 'loginUIDOMContentLoaded'}, '*');
});
