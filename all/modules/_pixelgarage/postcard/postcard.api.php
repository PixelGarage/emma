<?php
/**
 * Contains all postcard module specific hooks.
 *
 * Created by PhpStorm.
 * User: ralph
 * Date: 20.07.16
 * Time: 20:00
 */

/* --------------------------------------------------
 * Postcard multi-step webform
 * --------------------------------------------------*/
/**
 * Defines the multi-step webform process to create and deliver a postcard.
 * BE AWARE that the webform redirect url's are automatically set for each
 * webform in the process with a successor webform (url points to successor webform).
 *
 * The info array is an associated array defining all steps of the multi-step webform.
 *
 * A step info array must contain at least the following keys:
 *    'nid':          the webform node id
 *    'prev step':    step key of the previous step. Is null for the first step.
 *    'next step':    step key of the next step. IS null for the last step.
 */
function hook_postcard_multi_step_webform_info() {
  $info = array(
    'step1' => array(
      // set webform node id of step 1
      'nid' =>  1,
      // previous step
      'prev step' => null,
      // next step
      'next step' => null,
    ),
  );

  return $info;
}

/**
 * Alters the multi-step webform info to create and deliver a postcard.
 */
function hook_postcard_multi_step_webform_info_alter(&$info) {
  // set the node id of the first step
  $info['step 1']['nid'] = 345;

  // set a custom preview step after step 1
  $info['step 1']['next step'] = 'preview';
  $info['preview'] = array(
    // preview webform node id
    'nid' =>  346,
    // previous step
    'prev step' => 'step 1',
    // next step
    'next step' => null,
  );

  return $info;
}

/**
 * Alter a multi-step webform submission,  prior to saving it in the database.
 *
 * @param $step      string
 *    The key of the step.
 * @param $step_info array
 *    The step info array.
 * @param $node     object
 *    The webform node of the particular step
 * @param $submission   object
 *    The submission of the particular step
 */
function hook_postcard_multi_step_submission_presave_alter($step, $step_info, $node, &$submission) {
  
}
