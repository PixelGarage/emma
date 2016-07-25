<?php
/**
 * Created by PhpStorm.
 * User: ralph
 * Date: 06.02.15
 * Time: 17:22
 */

/**
 * Alters the menu entries.
 * @param $items
 */
function pixelgarage_menu_alter(&$items) {
  // hides local task tabs
  pg_login_menu_alter($items);
}


/* =============================================================================
 *    Proximity alter hooks implementation
 * ========================================================================== */

/**
 * Alters the proximity item load parameter array.
 *
 * The load parameter array defines a specific url parameter for each proximity item.
 * This parameter is added at the end of the request url and must be unique and URL conform.
 * The unique parameter defines, which item content has to be loaded from
 * the server (see next api function).
 *
 * @param $container_index      int     Index of proximity container (if more than one container exists in one page).
 * @param $view                 array   The view.
 * @param $ajax_load_params     array   Array of ajax load parameters to be altered, one for each proximity item (row)
 *                                      retrieved by the view. Default is the views row index.
 */
function pixelgarage_proximity_load_params_alter($container_index, $view, &$ajax_load_params) {
  // Return the node id as ajax parameter for each item.
  foreach ($view->result as $id => $item) {
    $ajax_load_params[$id] = $item->nid;
  }
}

/**
 * Implements the item specific content as render array or html string.
 * The input parameter $param contains the unique load parameter of the requested item.
 *
 * @param $container_index      int     Index of proximity container (if more than one container exists in one page).
 * @param $param                string  The item specific load parameter (see also hook_proximity_ajax_load_params_alter).
 * @param $render_item          mixed   The rendered content to be returned to the client. The $render_item should be
 *                                      replaced either by a string (rendered html content), a render array or an integer (error code).
 */
function pixelgarage_proximity_render_item_alter($container_index, $param, &$render_item) {
  // return the render array for the specific node, if available
  if ($node= node_load($param)) {
    $view_mode = 'full';
    if (property_exists($node, 'ds_switch') && !empty($node->ds_switch)) {
      // take an alternate view mode set by the ds switch
      $view_mode = $node->ds_switch;
    }
    $render_item = node_view($node, $view_mode);
  }
}


/* =============================================================================
 *     Multi-step webform hooks
 * ========================================================================== */

/**
 * Defines the available multi-step webform process(es).
 *
 * The process contains a postcard data collection step and a postcard delivery step.
 */
function pixelgarage_postcard_multi_step_webform_info_alter(&$info) {
  $info['postcard data'] = array(
    // webform node id
    'nid' =>  18,
    // previous step
    'prev step' => null,
    // next step id
    'next step' => 'postcard delivery',
    // submission related node type
    'related node type' => 'post'
  );
  $info['postcard delivery'] = array(
    // delivery webform node id
    'nid' =>  789,
    // previous step
    'prev step' => 'postcard data',
    // next step id
    'next step' => null,
  );

  return $info;
}

/**
 * Implements the postcard data collection submission presave step.
 *
 * A post node is created out of the submission and the node id added to the submission.
 */
function pixelgarage_postcard_multi_step_submission_presave_alter($step, $step_info, $node, &$submission) {
  //
  // process the multi-step webform submissions according to the step
  switch ($step) {
    case 'postcard data':
      _post_data_step_presave($step, $step_info, $node, $submission);
      break;
    case 'postcard delivery':
      _post_delivery_step_presave($step, $step_info, $node, $submission);
      break;
  }

}

function _post_data_step_presave($step, $step_info, $node, &$submission) {
  //
  // POSTCARD DATA SUBMISSION PRESAVE:
  // create or update submission related node with submitted values
  $master = postcard_webform_master_form($node);
  $status = (!empty($master->field_publish_post_immediately) && $master->field_publish_post_immediately[LANGUAGE_NONE][0]['value'] == 1) ? 1 : 0;

  //
  // check if a post exists already, and update or create the post accordingly
  $post_nid = false;
  foreach ($master->webform['components'] as $key => $data) {
    if ($data['form_key'] == 'post_nid') {
      $post_nid = !empty($submission->data[$key]) ? $submission->data[$key][0] : false;
    }
  }

  if ($post_nid) {
    // update existing post
    $post = node_load($post_nid);
    $post->status = 0;

  } else {
    // create post node and save it in the submission
    $post = new stdClass();
    $post->type = $step_info['related node type'];
    $post->language = LANGUAGE_NONE;
    $post->uid = 1;
    $post->status = 0;
    $post->comment = 0;
    $post->promote = 0;
    node_object_prepare($post);

  }
  //
  // update submission data on the offer and save it
  // fill the node properties and save it to the database
  $post->title = t('@you for @name', array('@you' => $submission->data[17][0], '@name' => $submission->data[4][0])); // create node title
  $post->field_name[$post->language][0]['value'] = $submission->data[4][0]; // child name
  $post->field_your_name[$post->language][0]['value'] = $submission->data[17][0]; // your name
  $post->field_quote[$post->language][0]['value'] = substr($submission->data[5][0], 0, 70); // Quote
  $post->field_image[$post->language][0]['fid'] = $submission->data[1][0]; // fid image
  $post->field_hires_image[$post->language][0]['fid'] = $submission->data[1][0]; // fid image (for high resolution image)
  $post = node_submit($post);
  node_save($post);

  //
  // add related post id to submission
  foreach ($master->webform['components'] as $key => $data) {
    if ($data['form_key'] == 'post_nid') {
      $submission->data[$key][0] = $post->nid;
      break;
    }
  }
  
  //
  // add post node to session (with status)
  $session_data = &pixelgarage_session_data();
  $session_data['post_info'] = array(
    'nid' => $post->nid,
    'status' => $status,
  );
  
  //
  // add postcard to next webform node
  

}

function _post_delivery_step_presave($step, $step_info, $node, $submission) {
  //
  // POSTCARD DELIVERY SUBMISSION PRESAVE:
  // multi-step form is finished, publish post
  $session_data = &pixelgarage_session_data();
  $post_info = $session_data['post_info'];

  if (!empty($post_info)) {
    //
    // publish existing post
    $post = node_load($post_info['nid']);
    $post->status = $post_info['status'] ? $post_info['status'] : 0;
    $post->field_e_mail[$post->language][0]['email'] = $submission->data[20][0]; // email
    node_save($post);

    //
    // update submission with postcard data
  }

  // cleanup session dat
  unset($session_data['post_info']);

}
