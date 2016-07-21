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
  // get the master form (translation by string)
  $master = postcard_webform_master_form($node);

  //
  // perform the submission presave for the specific steps
  $steps = _postcard_multi_step_webform_info();
  foreach($steps as $key => $step_info) {
    // load step webform master
    $step_form = node_load($step_info['nid']);
    $step_master = postcard_webform_master_form($step_form);

    //
    // handle submission presave of step
    if ($master->nid == $step_master->nid) {
      //
      // alter submission before saving
      drupal_alter('postcard_multi_step_submission_presave', $key, $step_info, $step_master, $submission);

      //
      // keep webform and submission id for next step
      $session_data = &postcard_session_data();
      $session_data[$key] = array(
        'nid' => $step_master->nid,
        'sid' => $submission->sid,
      );
      
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

      break;
    }
  }
}

/**
 * Implements hook_form_FORM_ID_alter().
 *
 * Extends the webform client form with a back button, if a previous step exists.
 */

function postcard_form_webform_client_form_alter (&$form, &$form_state) {
  $node = $form['#node'];
  $submission = $form['#submission'];
  
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

    //
    // add redirect url to step webform (next step's webform)
    foreach($info as $key => $step_info) {
      // load step webform master
      $step_form = node_load($step_info['nid']);
      $step_master = postcard_webform_master_form($step_form);

      //
      // add redirect url to step webform (points to next step's webform)
      if ($step_info['next step']) {
        $next_nid = $steps[$step_info['next step']]['nid'];
        $step_master->webform['redirect_url'] = "node/{$next_nid}";
        node_save($step_master);
      }
    }

  }
  return $info;
}

/**
 * Gets the node and submission id of the previous step submission in a multi-step webform.
 *
 * @param $nid  int   Node id of current webform.
 * @param $sid  int   Submission id of current submission.
 * @return array mixed
 *    Array with nid and sid of previous step or false, if no result has been found.
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


