<?php
/**
 * Enhances the webform module with the possibility to define multi-step webforms.
 *
 * The following functionality is available:
 *    - definition of multi-step webforms can be done with hooks
 *    - automatic creation of webform sequence according to the definition (redirect_url)
 *    - automatic creation of Back-buttons in webforms, if a previous webform exists.
 *    - muti-step webform editing by anonymous user supported (access via token)
 *    - submission_presave hook is multi-step enabled
 *
 * Created by PhpStorm.
 * User: ralph
 * Date: 20.07.16
 * Time: 17:36
 */


/* --------------------------------------------------
 * Webform hooks
 * --------------------------------------------------*/

/**
 * Called for each webform submission before it is saved.
 *
 * Implements a multi-step form submission presave.
 */
function postcard_webform_submission_presave($node, &$submission) {
  //
  // perform the submission presave for the specific step (webform node)
  $master = postcard_webform_master_form($node);
  $step_info = _get_step_info($master);
  $step_form = node_load($step_info['nid']);
  $step_master = postcard_webform_master_form($step_form);
  $step_key = _get_step_key($master);

  //
  // alter submission before saving
  drupal_alter('postcard_multi_step_submission_presave', $step_key, $step_info, $step_master, $submission);

  //
  // save webform and submission id in session for next step
  $session_data = &postcard_session_data();
  $session_data[$step_key] = array(
    'nid' => $step_master->nid,
    'sid' => $submission->sid,
  );
  $session_data['last_step'] = $step_key;

  //
  // store previous submission permanently to database, if any
  $prev_step = $step_info['prev step'];
  if ($prev_step && isset($session_data[$prev_step])) {
    $record  = array(
      'nid' => $step_master->nid,
      'sid' => $submission->sid,
      'prev nid' => $session_data[$prev_step]['nid'],
      'prev sid' => $session_data[$prev_step]['sid'],
    );
    drupal_write_record('postcard_webform_multistep', $record);
  }

  // delete session data for last step
  if (!$step_info['next step']) {
    unset($session_data);
  }
}

/**
 * Implements hook_form_FORM_ID_alter().
 *
 * Extends the webform client form with a back button, if a previous step exists.
 */

function postcard_form_webform_client_form_alter (&$form, &$form_state) {
  $session_data = &postcard_session_data();
  $node = $form['#node'];
  $master = postcard_webform_master_form($node);
  $submission = $form['#submission'];

  //
  // find the previous / next step (webform) of the multi-step webform
  $prev_step = false;
  $next_step = false;
  if ($submission) {
    //
    // OPENED VIA ANONYMOUS ACCESS LINK
    // get previous and next step from database, if available
    $prev_step = _get_previous_step_submission($master->nid, $submission->sid);
    $next_step = _get_next_step_submission($master->nid, $submission->sid);

  }
  else if (isset($session_data['last_step'])) {
    //
    // CALLED DURING MULTI-STEP PROCESS
    // get the previous step from session (no db entry yet)
    $prev_step = $session_data[$session_data['last_step']];
  }

  //
  // define the back link, if a previous webform exists
  if ($prev_step) {
    $form['actions']['back'] = array(
      '#markup' => postcard_create_anonym_submission_link($prev_step['nid'], $prev_step['sid'], 'anonym_edit'),
      '#attributes' => array(
        'class' => array('back-link', 'button-primary'),
      ),
    );
  }
  
  //
  // set the language dependent redirect url of the form. Set anonymous link, if next step is known!
  if ($next_step) {
    $form['#node']->webform['redirect_url'] =
      postcard_create_anonym_submission_link($next_step['nid'], $next_step['sid'], 'anonym_edit', false);

  }
  else if ($next_step = _get_step_info($master, 'next')) {
    global $language;

    $next_form = node_load($next_step['nid']);
    $next_master = postcard_webform_master_form($next_form);
    $transl_arr = translation_node_get_translations($next_master->nid);
    $lang_nid = !empty($transl_arr) ? $transl_arr[$language->language]->nid : $next_master->nid;

    $form['#node']->webform['redirect_url'] = "node/{$lang_nid}";

  }
}


/* --------------------------------------------------
 * Postcard multi-step webform helper
 * --------------------------------------------------*/
/**
 * Load postcard multi-step webform info from 3rd party modules.
 */
function _postcard_multi_step_webform_info() {
  $info = &drupal_static(__FUNCTION__);
  if (!isset($info)) {
    $info = array();

    foreach (module_implements('postcard_multi_step_webform_info') as $module) {
      $steps = module_invoke($module, 'postcard_multi_step_webform_info');
      foreach ($steps as $key => $step) {
        $steps[$key]['module'] = $module;
      }
      $info = array_merge($info, $steps);
    }
    drupal_alter('postcard_multi_step_webform_info', $info);

  }
  return $info;
}

/**
 * Gets the step key of the given master node (language independent)
 *
 * @param $master   object
 *    The master webform node (tnid)
 * @param $step   string
 *     Step key to return, e.g. 'current' | 'prev' | 'next', default is 'current'
 * @return string
 *    The step key string.
 */
function _get_step_key($master, $step = 'current') {
  $steps = _postcard_multi_step_webform_info();
  foreach($steps as $key => $step_info) {
    // load step webform master
    $step_form = node_load($step_info['nid']);
    $step_master = postcard_webform_master_form($step_form);

    if ($master->nid == $step_master->nid) {
      switch ($step) {
        case 'current':
          return $key;
        case 'prev':
          $key = $step_info['prev step'];
          return $key ? $key : false;
        case 'next':
          $key = $step_info['next step'];
          return $key ? $key : false;
      }
    }
  }
  return false;
}

/**
 * Gets the step info array of the given master node (language independent)
 *
 * @param $master   object
 *    The master webform node (tnid)
 * @param $step   string
 *     Step info to return, e.g. 'current' | 'prev' | 'next', default is 'current'
 * @return bool|mixed
 */
function _get_step_info($master, $step = 'current') {
  $steps = _postcard_multi_step_webform_info();
  foreach($steps as $key => $step_info) {
    // load step webform master
    $step_form = node_load($step_info['nid']);
    $step_master = postcard_webform_master_form($step_form);

    if ($master->nid == $step_master->nid) {
      switch ($step) {
        case 'current':
          return $step_info;
        case 'prev':
          $key = $step_info['prev step'];
          return $key ? $steps[$key] : false;
        case 'next':
          $key = $step_info['next step'];
          return $key ? $steps[$key] : false;
      }
    }
  }
  return false;
}

/**
 * Gets the node and submission id of the previous step in a multi-step webform from the database.
 *
 * @param $nid  int   Node id of current webform.
 * @param $sid  int   Submission id of current submission.
 * @return array mixed
 *    Associated array with nid and sid of previous step or false, if no result has been found.
 */
function _get_previous_step_submission($nid, $sid) {
  //
  // Query the previous node and submission id.
  $query = db_select('postcard_webform_multistep', 'wm');
  $query
    ->fields('wm', array('prev_nid', 'prev_sid'))
    ->condition('wm.nid', $nid)
    ->condition('wm.sid', $sid)
    ->range(0, 1);
  $result = $query->execute();

  $prev_step_submission = array();
  foreach ($result as $row) {
    $prev_step_submission['nid'] = $row->prev_nid;
    $prev_step_submission['sid'] = $row->prev_sid;
  }

  return !empty($prev_step_submission) ? $prev_step_submission : false;
}

/**
 * Gets the node and submission id of the next step in a multi-step webform from the database.
 *
 * @param $nid  int   Node id of current webform.
 * @param $sid  int   Submission id of current submission.
 * @return array mixed
 *    Associated array with nid and sid of next step or false, if no result has been found.
 */
function _get_next_step_submission($nid, $sid) {
  //
  // Query the next node and submission id.
  $query = db_select('postcard_webform_multistep', 'wm');
  $query
    ->fields('wm', array('nid', 'sid'))
    ->condition('wm.prev_nid', $nid)
    ->condition('wm.prev_sid', $sid)
    ->range(0, 1);
  $result = $query->execute();

  $prev_step_submission = array();
  foreach ($result as $row) {
    $prev_step_submission['nid'] = $row->prev_nid;
    $prev_step_submission['sid'] = $row->prev_sid;
  }

  return !empty($prev_step_submission) ? $prev_step_submission : false;
}



/* ---------------------------------------------------------------------
 * Webform submission access by anonymous user (offer actions, access rights)
 * ---------------------------------------------------------------------*/

/**
 * Implements hook_webform_webform_submission_actions().
 *
 * Adds actions to edit or delete the corresponding submission by an anonymous user (token enabled access).
 */
function postcard_webform_submission_actions($node, $submission) {
  $actions = array();
  $token_access = $submission && isset($_GET['token']) && $_GET['token'] == webform_get_submission_access_token($submission);

  if ($token_access) {
    //
    // show submission actions only for anonymous users with token access
    // and secure the action callbacks with token too
    $actions['view'] = array(
      'title' => t('View submission'),
      'href' => 'postcard/' . $node->nid . '/submission/' . $submission->sid . '/anonym_view',
      'query' => drupal_get_query_parameters(),
    );
    $actions['edit'] = array(
      'title' => t('Edit submission'),
      'href' => 'postcard/' . $node->nid . '/submission/' . $submission->sid . '/anonym_edit',
      'query' => drupal_get_query_parameters(),
    );
    $actions['remove'] = array(
      'title' => t('Remove submission'),
      'href' => 'postcard/' . $node->nid . '/submission/' . $submission->sid . '/anonym_remove',
      'query' => drupal_get_query_parameters(),
    );
  }

  return $actions;
}

/**
 * Confirm form to delete a single form submission.
 *
 * @param $form
 *   The new form array.
 * @param $form_state
 *   The current form state.
 * @param $master
 *   The master webform of a translation set.
 * @param $submission
 *   The submission to be deleted (including the attached offer).
 * @return mixed The confirmation form render array.
 */
function postcard_webform_submission_remove_form($form, $form_state, $master, $submission) {
  // Keep the NID and SID in the same location as the webform_client_form().
  // This helps mollom identify the same fields when deleting a submission.
  $form['#tree'] = TRUE;
  $form['details']['nid'] = array(
    '#type' => 'value',
    '#value' => $master->nid,
  );
  $form['details']['sid'] = array(
    '#type' => 'value',
    '#value' => $submission->sid,
  );

  $question = t('Are you sure you want to remove this submission?');
  $query = drupal_http_build_query(array('token' => $_GET['token']));
  $cancel_redirect = "postcard/{$master->nid}/submission/{$submission->sid}/anonym_view" . '?' . $query;

  return confirm_form($form, NULL, $cancel_redirect, $question, t('Remove'), t('Cancel'));
}

function postcard_webform_submission_remove_form_submit($form, &$form_state) {
  $master = node_load($form_state['values']['details']['nid']);
  $submission = webform_get_submission($form_state['values']['details']['nid'], $form_state['values']['details']['sid']);
  $post_nid = _webform_submission_value($master, 'post_nid', $submission);

  // unpublish post
  $post = node_load($post_nid);
  $post->status = 0;
  node_save($post);

  drupal_set_message(t('Submission successfully removed.'));

  // redirect to home
  $form_state['redirect'] = '/';
}


/* --------------------------------------------------
 * Webform submission helpers
 * --------------------------------------------------*/
/**
 * Gets the submitted value for a specific webform component.
 *
 * @param object $master
 *    The webform master node containing the component to be read.
 * @param string $form_key
 *    The defined form key of the component.
 * @param object $submission
 *    The form submission with the submitted values.
 * @return string The value of the webform component or an empty string, if not set.
 *    The value of the webform component or an empty string, if not set.
 */
function _webform_submission_value($master, $form_key, $submission) {
  foreach ($master->webform['components'] as $key => $data) {
    if ($data['form_key'] == $form_key) {
      return !empty($submission->data[$key]) ? $submission->data[$key][0] : '';
    }
  }
}

/**
 * Sets a submission value for the given form key.
 *
 * @param object $master
 *    The webform master node containing the component to be updated.
 * @param $submission    object
 *    The submission object
 * @param $form_key      string
 *    The form key of the component
 * @param $value         mixed
 *    The value to be set on submission for specified component
 */
function _webform_set_submission_value($master, $form_key, $submission, $value) {
  foreach ($master->webform['components'] as $key => $data) {
    if ($data['form_key'] == $form_key) {
      $submission->data[$key][0] = $value;
      break;
    }
  }
}


