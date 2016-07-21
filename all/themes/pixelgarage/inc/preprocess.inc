<?php
/**
 * Created by PhpStorm.
 * User: ralph
 * Date: 06.02.15
 * Time: 17:23
 */

/**
 * Preprocess variables for the html template.
 */
function pixelgarage_preprocess_html(&$vars) {
  // make sure jQuery UI and effects is loaded for anonymous users
  drupal_add_library('system', 'ui');
  drupal_add_library('system', 'effects');

}

/**
 * Override or insert variables for the page templates.
 */
function pixelgarage_preprocess_page(&$vars) {
  // hide titles on login forms
  pg_login_preprocess_page($vars);

  // replace logo with .svg
  $vars['logo'] = str_replace('.jpg', '.svg', $vars['logo']);

  // add node category class, if node = 'post' in full mode
  if (isset($vars['node']) && $vars['node']->type == 'post') {
    $node = $vars['node'];
    $vars['post_category_class'] = '';
    $tid = (!empty($node->field_category) && $node->field_category[LANGUAGE_NONE][0]['tid']) ? $node->field_category[LANGUAGE_NONE][0]['tid'] : null;
    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'taxonomy_term')
      ->entityCondition('bundle', 'category')
      ->propertyCondition('tid', $tid);
    $result = $query->execute();

    if (isset($result['taxonomy_term'])) {
      $tids = array_keys($result['taxonomy_term']);
      $terms = taxonomy_term_load_multiple($tids);
      foreach ($terms as $term) {
        $vars['post_category_class'] = ' class="' . drupal_clean_css_identifier( $term->field_css_class[LANGUAGE_NONE][0]['value']) . '"';
      }
    }
  }
}

/**
 * Implements template_preprocess_views_exposed_form().
 *
 * Adds total result count to exposed form and the placeholder for the search field.
 */
function pixelgarage_preprocess_views_exposed_form(&$vars) {
  $view = views_get_page_view();
  $vars['total_rows'] = null;
  if ($filter = &$vars['widgets']['filter-combine']) {
    $placeholder = 'placeholder="' . $filter->label . '"/>';
    $filter->widget = str_replace('/>', $placeholder, $filter->widget);
  }

  if (empty($vars['form']['combine']['#value'])) return;

  $page_total = count($view->result);
  if(isset($view->total_rows)){
    $text = ($view->total_rows == 1) ? '%hits hit' : '%hits hits';
    $vars['total_rows'] = t($text, array('%hits' => $view->total_rows));
  } else {
    $text = ($page_total == 1) ? '%hits hit' : '%hits hits';
    $vars['total_rows'] = t($text, array('%hits' => $page_total));
  }
}

/**
 * Implements template_preprocess_webform_confirmation().
 */
function pixelgarage_preprocess_webform_confirmation(&$vars) {
  // because we have translated webform nodes, we have to get the submissions
  // of the original node of the translation set, set in the "tnid" property
  if (!isset($vars['node']) || !isset($vars['sid'])) return;

  $webform = $vars['node'];
  $tnid = $webform->tnid ? $webform->tnid : $webform->nid;

  $submissions = webform_get_submissions(array('nid' => $tnid, 'sid' => $vars['sid']));
  $submission = $submissions[$vars['sid']];

  // get node and render it
  $term = taxonomy_term_load($submission->data[11][0]); // category tid
  $vars['post_category_class'] = drupal_clean_css_identifier( $term->field_css_class[LANGUAGE_NONE][0]['value']);

  $post = node_load($submission->data[16][0]);
  $vars['post'] = node_view($post, 'full');
}


/* =============================================================================
 *      Front Masonry
 * ========================================================================== */
/**
 * Adapts the proximity view, so that always two randomly chosen testimonials are visible
 * in the masonry.
 */
function pixelgarage_preprocess_views_view_proximity (&$vars) {
  //$vars['testimonials'] = _random_testimonials();
}

function _random_testimonials() {
  static $testimonials = array();
  if (empty($testimonials)) {
    // get the published testimonials
    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', 'testimonial')
      ->propertyCondition('status', NODE_PUBLISHED);
    $result = $query->execute();
    if ($result && !empty($result['node'])) {
      $testimonials = node_load_multiple(array_keys($result['node']));
    }
  }

  // return a random testimonial
  $keys = array_rand($testimonials, 2);
  $i1 = mt_rand(0, 9);
  return array(
    0 => node_view($testimonials[$keys[0]], 'teaser'),
    $i1 => node_view($testimonials[$keys[1]], 'teaser'),
  );
}

