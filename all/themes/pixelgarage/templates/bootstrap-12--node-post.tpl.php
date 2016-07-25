<?php
/**
 * @file
 * Bootstrap 12 template for Display Suite.
 */
//
// create language dependent banner and slogan images
global $language;

switch ($language->language) {
  case 'de':
  default:
    $banner_path = drupal_get_path('theme', 'pixelgarage') . '/images/logo_banner_d.png';
    $slogan_path = drupal_get_path('theme', 'pixelgarage') . '/images/post_header_d.svg';
    break;
  case 'fr':
    $banner_path = drupal_get_path('theme', 'pixelgarage') . '/images/logo_banner_d.png';
    $slogan_path = drupal_get_path('theme', 'pixelgarage') . '/images/post_header_d.png';
    break;
}
$params = array(
  'path' => $banner_path,
  'alt' => 'for you',
  'width' => '90%',
  'height' => 'auto',
  'attributes' => array('class' => array('img', 'img-banner')),
);
$banner_img = theme_image($params);

$slogan_params = array(
  'path' => $slogan_path,
  'width' => '100%',
  'height' => 'auto',
  'alt' => 'sustainable until 2050',
  'attributes' => array('class' => array('img', 'img-slogan')),
);
$slogan_img = theme_image($slogan_params);

//
// set language dependent heading
$post_title = t('Do you like it?');
$post_heading = t('Share this post with your friends and family members!')
?>


<?php if (!$teaser): ?>
<div class="node-post-page-wrapper">
    <div class="node-post-header">
      <div class="post-title">
        <?php print $post_title ?>
      </div>
      <div class="post-heading">
        <?php print $post_heading ?>
      </div>
      <div class="social-share">
        <!-- AddtoAny social share buttons -->
        <div class="a2a_kit a2a_kit_size_32 a2a_default_style">
          <a class="a2a_button_facebook"></a>
          <a class="a2a_button_twitter"></a>
          <a class="a2a_button_google_plus"></a>
        </div>
        <script async src="https://static.addtoany.com/menu/page.js"></script>
      </div>
    </div>
    <div class="node-post-wrapper">
<?php endif; ?>

<<?php print $layout_wrapper; print $layout_attributes; ?> class="<?php print $classes; ?>">
  <?php if (isset($title_suffix['contextual_links'])): ?>
    <?php print render($title_suffix['contextual_links']); ?>
  <?php endif; ?>
  <div class="row">
    <<?php print $central_wrapper; ?> class="col-sm-12 <?php print $central_classes; ?>">
      <?php print $central; ?>
      <div class="slogan-wrapper"><?php print $slogan_img; ?></div>
      <div class="banner-wrapper"><?php print $banner_img; ?></div>
    </<?php print $central_wrapper; ?>>
  </div>
</<?php print $layout_wrapper ?>>

<?php if (!$teaser): ?>
  <!-- close node wrapper and node-page-wrapper-->
  </div>
</div>
<?php endif; ?>



<!-- Needed to activate display suite support on forms -->
<?php if (!empty($drupal_render_children)): ?>
  <?php print $drupal_render_children ?>
<?php endif; ?>
