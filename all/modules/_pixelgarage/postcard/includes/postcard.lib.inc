<?php
/**
 * This library contains all functionality to create a postcard.
 * The postcard is created as PDF (tcpdf needed) and can be converted to a PNG in high resolution (ImageMagick needed).
 * 
 * 
 * Created by PhpStorm.
 * User: ralph
 * Date: 28.07.16
 * Time: 12:37
 */

/**
 * Creates a postcard in pdf format from a post node.
 * 
 * This function needs the tcpdf_document module installed.
 * @see tcpdf_document_instance()
 */
function postcard_create_pdf(&$node) {
  //
  // create the postcard file uri and path
  $uri = 'private://postcards';
  $filename = _file_check_name($node->title . '_' . time() . '.pdf');
  $file_uri = $uri . '/' . $filename;
  $file_sw = file_stream_wrapper_get_instance_by_uri($file_uri);
  $filepath = $file_sw->realpath();

  //
  // create hires image from node image (pdf resolution), if possible
  $style = image_style_load('post_hires_image');
  $image_fid = $node->field_image[$node->language][0]['fid'];
  $node_img = file_load($image_fid);

  if (!empty($style) && $node_img) {
    $hires_img_uri = image_style_path($style['name'], $node_img->uri);
    if (!image_style_create_derivative($style, $node_img->uri, $hires_img_uri)) {
      // hires image could not be created, take lowres image
      $hires_img_uri = image_style_path('post_image', $node_img->uri);
    }
  }
  else {
    // no image or style available, don't create pdf
    watchdog('Postcard PDF creation', 'No user image file available. PDF not created', array(), WATCHDOG_ALERT);
    return;
  }
  $hires_img_sw = file_stream_wrapper_get_instance_by_uri($hires_img_uri);
  $hires_img_path = $hires_img_sw->realpath();

  //
  // get banner image path from theme
  global $language;
  switch($language->language) {
    case 'de':
    default:
      $ext = '_d.png';
      break;
    case 'fr':
      $ext = '_f.png';
      break;
    case 'en':
      $ext = '_e.png';
      break;
  }
  $banner_img_path = drupal_get_path('theme', 'pixelgarage') . '/images/logo_banner' . $ext;

  //
  // create strings from node
  $name = $node->field_name[$node->language][0]['value'];
  $your_name = $node->field_your_name[$node->language][0]['value'];
  $quote = $node->field_quote[$node->language][0]['value'];
  $slogan = t('- Nachhaltig bis 2050 -');


  //
  // create postcard and save it as pdf
  $options = array(
    'title' => 'Postcard created by FürDich.org',
    'subject' => 'Ja zur Grünen Wirtschaft',
    'author' => 'FürDich.org',
    'keywords' => 'Postcard, Grüne Wirtschaft, tcpdf, PDF, Drupal, Pixelgarage',
    'header' => null,
    'footer' => null,
  );
  $pdf = tcpdf_document_instance($options, 'P', 'mm', 'A6'); //105 x 148 mm

  // fill background with green (disable auto-page-break)
  $bMargin = $pdf->getBreakMargin();
  $pdf->SetAutoPageBreak(false, 0);
  $bg_color = array(155, 195, 27); // #9bc31b
  $pdf->Rect(0, 0, 105, 148, 'F', array(), $bg_color);  //
  
  // draw post image (high-resolution)
  $pdf->Image($hires_img_path, 0, 0, 105, 105, '', '', '', false, 300, '', false, false, 0, 'CT');

  // restore auto-page-break status
  $pdf->SetAutoPageBreak(true, $bMargin);
  $pdf->setPageMark();


  // write pdf to file
  $pdf->lastPage();
  $pdf->Output($filepath, 'F');


  //
  // save pdf file (managed)
  $file = new stdClass();
  $file->fid = NULL;
  $file->uri = $file_uri;
  $file->filename = $filename;
  $file->filemime = file_get_mimetype($file_uri);
  $file->status   = FILE_STATUS_PERMANENT;
  $file->uid      = 1;
  $file = file_save($file);

  $node->field_postcard[$node->language][0]['fid'] = $file->fid;
  $node->field_postcard[$node->language][0]['display'] = 1;

  //
  // convert postcard pdf to image
  //_postcard_convert_to_image($node, '.png');

  //
  node_save($node);
}

/**
 * Helper function to created a valid file name.
 */
function _file_check_name($name) {
  // Punctuation characters that are allowed, but not as first/last character.
  $punctuation = '-_.';
  $map = array(
    // Replace (groups of) whitespace characters.
    '!\s+!' => ' ',
    // Replace multiple dots.
    '!\.+!' => '.',
    // Remove characters that are not alphanumeric or the allowed punctuation.
    "![^0-9A-Za-z$punctuation]!" => '',
    );

  // Apply the regex replacements. Remove any leading or hanging punctuation.
  return trim(preg_replace(array_keys($map), array_values($map), $name), $punctuation);
}

/**
 * Converts a pdf file into a high resolution image file of given format.
 *
 * @param $node
 *    The node holding the pdf to be converted
 * @param $img_format string
 *    The file extension of the requested image format, e.g. '.png', '.jpg'
 */
function _postcard_convert_to_image(&$node, $img_format) {
  //
  // get pdf file form node
  $pdf_fid = $node->field_postcard[$node->language][0]['fid'];
  if (!$pdf_fid) return;

  //
  // create image path from pdf path
  $pdf_file = file_load($pdf_fid);
  $img_uri = str_replace('.pdf', $img_format, $pdf_file->uri);
  $pdf_sw = file_stream_wrapper_get_instance_by_uri($pdf_file->uri);
  $pdfpath = $pdf_sw->realpath();
  $img_sw = file_stream_wrapper_get_instance_by_uri($img_uri);
  $imgpath = $img_sw->realpath();

  $dpi = 300;
  $quality = 75;

  //
  // set the convert arguments for a high quality conversion
  // and execute command (ImageMagick convert command)
  //
  // ImageMagick arguments: convert { [-setting]... "image"|-operation }... "output_image"
  // @see http://www.imagemagick.org/Usage/basics/#cmdline
  $args_String = "-density {$dpi} -trim {$pdfpath} -quality {$quality} -sharpen 0x1.0 {$imgpath}";
  $convert_args = escapeshellarg($args_String);
  if (_imagemagick_convert_exec($convert_args)) {
    // store created hires image in node
    $img = new stdClass();
    $img->fid = NULL;
    $img->uri = $img_uri;
    $img->filename = trim(drupal_basename($img_uri));
    $img->filemime = file_get_mimetype($img_uri);
    $img->status   = FILE_STATUS_PERMANENT;
    $img = file_save($img);

    $node->field_hires_image[$node->language][0]['fid'] = $img->fid;
  }

}


/* =============================================================================
 *   Helper functions to show pdf in browser
 * ========================================================================== */

/**
 * Menu callback of a PDF creation request.
 *
 * @param $nid int
 *    Node ID of which the postcard pdf has to be created.
 */
function _postcard_show_pdf($nid)
{
  // load post node
  $post = node_load($nid);

  // create the pdf
  postcard_create_pdf($post);

  // get created pdf file from node
  $pdf_fid = $post->field_postcard[$post->language][0]['fid'];
  if ($pdf_file = file_load($pdf_fid)) {
    _postcard_transfer_pdf($pdf_file->uri, $pdf_file->filename, true);
  }

  // file not found
  drupal_not_found();
  drupal_exit();
}

/**
 * Downloads / shows the pdf with the given file name from the private folder.
 *
 * @param string $pdf_uri
 *    A file uri in the format "scheme://target".
 * @param string $filename
 *    The filename of the file.
 * @param bool $show_file_inline
 *    Flag, if TRUE shows the file inline (in the browser), otherwise downloads the file to the client.
 * @param int $valid_duration
 *    Duration during which the file is valid (default: -1 meaning no time limit)
 */
function _postcard_transfer_pdf($pdf_uri, $filename, $show_file_inline = false, $valid_duration = -1)
{
  // get stream wrapper instance for pdf
  $stream_wrapper = file_stream_wrapper_get_instance_by_uri($pdf_uri);
  $pdf_path = $stream_wrapper->realpath();

  if (file_exists($pdf_path)) {
    if ($valid_duration < 0 || filemtime($pdf_path) >= time() - $valid_duration) {
      $content_dispo = $show_file_inline ? 'inline' : 'attachment';
      $content_dispo .= '; filename="' . $filename . '"';
      file_transfer($pdf_uri, array(
        'Cache-control' => 'private',
        'Content-Type' => file_get_mimetype($pdf_uri), //'application/pdf',  'application/octet-stream'
        'Content-Disposition' => $content_dispo,
        'Content-Length' => filesize($pdf_path)
      ));
    }
  }
  drupal_not_found();
  drupal_exit();
}



/* =============================================================================
 *   ImageMagick convert wrapper
 * ========================================================================== */

/**
 * Executes the ImageMagick convert executable as shell command.
 *
 * @param $command_args
 *   A string containing arguments to pass to the convert command, which must
 *   have been passed through escapeshellarg() already.
 * @param &$output
 *   (optional) A variable to assign the shell stdout to, passed by reference.
 * @param &$error
 *   (optional) A variable to assign the shell stderr to, passed by reference.
 * @param $convert_path
 *   (optional) A custom file path to the convert binary. Internal use only.
 *
 * @return mixed
 *   The return value depends on the shell command result:
 *   - Boolean TRUE if the command succeeded.
 *   - Boolean FALSE if the shell process could not be executed.
 *   - Error exit status code integer returned by the executable.
 */
function _imagemagick_convert_exec($command_args, &$output = NULL, &$error = NULL, $convert_path = '/usr/local/bin') {
  // $convert_path is only passed from the system-wide image toolkit form, on
  // which the path to convert is configured.
  // @see _imagemagick_check_path()
  if (!isset($convert_path)) {
    // By using a default of NULL, we force users to setup the toolkit through
    // the image toolkit administration UI. Sites enforcing a path via
    // settings.php should know what they are doing.
    $convert_path = variable_get('imagemagick_convert_path', NULL);
    if (!isset($convert_path)) {
      return FALSE;
    }
  }

  // Use Drupal's root as working directory to resolve relative paths correctly.
  $drupal_path = DRUPAL_ROOT;

  if (strstr($_SERVER['SERVER_SOFTWARE'], 'Win32') || strstr($_SERVER['SERVER_SOFTWARE'], 'IIS')) {
    // Use Window's start command with the /B flag to make the process run in
    // the background and avoid a shell command line window from showing up.
    // @see http://us3.php.net/manual/en/function.exec.php#56599
    // Use /D to run the command from PHP's current working directory so the
    // file paths don't have to be absolute.
    $convert_path = 'start "ImageMagick" /D ' . escapeshellarg($drupal_path) . ' /B ' . escapeshellarg($convert_path);
  }
  $command = $convert_path . ' ' . $command_args;

  $descriptors = array(
    // stdin
    0 => array('pipe', 'r'),
    // stdout
    1 => array('pipe', 'w'),
    // stderr
    2 => array('pipe', 'w'),
  );
  if ($h = proc_open($command, $descriptors, $pipes, $drupal_path)) {
    $output = '';
    while (!feof($pipes[1])) {
      $output .= fgets($pipes[1]);
    }
    $error = '';
    while (!feof($pipes[2])) {
      $error .= fgets($pipes[2]);
    }

    fclose($pipes[0]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    $return_code = proc_close($h);

    // Display debugging information to authorized users.
    if (variable_get('imagemagick_debug', FALSE) && user_access('administer site configuration')) {
      debug($command, t('ImageMagick command'), TRUE);
      if ($output !== '') {
        debug($output, t('ImageMagick output'), TRUE);
      }
      if ($error !== '') {
        debug($error, t('ImageMagick error'), TRUE);
      }
    }

    // If ImageMagick returned a non-zero code, trigger a PHP error that will
    // be caught by Drupal's error handler, logged to the watchdog and
    // eventually displayed to the user if configured to do so.
    if ($return_code != 0) {
      // If there is no error message, clarify this.
      if ($error === '') {
        $error = t('No error message.');
      }
      // Format $error with as full message, passed by reference.
      $error = t('ImageMagick error @code: !error', array(
        '@code' => $return_code,
        '!error' => $error,
      ));
      watchdog('Postcard creation', 'ImageMagick error @code: !error', array('@code' => $return_code, '!error' => $error), WATCHDOG_ERROR);
      return $return_code;
    }
    // The shell command was executed successfully.
    return TRUE;
  }
  // The shell command could not be executed.
  watchdog('Postcard creation', 'The ImageMagick command could not be executed due to an unknown error.', array(), WATCHDOG_ERROR);
  return FALSE;
}

