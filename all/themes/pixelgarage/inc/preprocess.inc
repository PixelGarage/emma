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
}


/* =============================================================================
 *      Exposed form
 * ========================================================================== */
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


/* =============================================================================
 *      Front Masonry
 * ========================================================================== */
/**
 * Adapts the proximity view, so that always two randomly chosen testimonials are visible
 * in the masonry.
 */
function pixelgarage_preprocess_views_view_proximity (&$vars) {
  $vars['testimonials'] = _random_testimonials($vars);
}

function _random_testimonials(&$vars) {
  static $testimonials = array();
  if (empty($testimonials)) {
    global $language;
    // get the published testimonials
    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', 'testimonial')
      ->propertyCondition('status', NODE_PUBLISHED)
      ->propertyCondition('language', $language->language);
    $result = $query->execute();
    if ($result && !empty($result['node'])) {
      $testimonials = node_load_multiple(array_keys($result['node']));
    }
  }

  // chose random testimonial from list
  $keys = array_keys($testimonials);
  $row_count = $vars['view']->total_rows - 1;
  $i1 = mt_rand(0, $row_count);
  $i2 = mt_rand(0, $row_count);
  $i3 = mt_rand(0, $row_count);

  // return the testimonials at random place in grid
  return array(
    $i1 => node_view($testimonials[$keys[0]], 'teaser'),
    $i2 => node_view($testimonials[$keys[1]], 'teaser'),
    $i3 => node_view($testimonials[$keys[2]], 'teaser'),
  );
}

