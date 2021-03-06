<?php
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

require_once("parapara.inc");

function getFilenameForId($id)
{
  global $config;
  return $config['characters']['path'] . $id . ".svg";
}

function getGalleryUrlForId($id)
{
  // Return the raw url without the prefix which we map to the gallery page with
  // a rewrite rule
  return substr(getRawUrlForId($id), 0, -4);
}

function getRawUrlForId($id)
{
  return fileToUrl(getFilenameForId($id));
}

function shortenUrl($url)
{
  global $config;
  if (!isset($config['bitly']) ||
      !@$config['bitly']['login'] ||
      !@$config['bitly']['apiKey'])
    return $url;
  $login  = $config['bitly']['login'];
  $apiKey = $config['bitly']['apiKey'];
  $shorten_url = "http://api.bit.ly/v3/shorten?login=$login&apiKey=$apiKey" .
    "&uri=" . urlencode($url) . "&format=txt";
  $shortened = getUrl($shorten_url);
  // If there is an error, return the original url
  return $shortened ? trim($shortened) : $url;
}

function getUrl($url, $data = null) {
  return fetchUrl($url, $data, 'get');
}

function postUrl($url, $data = null) {
  return fetchUrl($url, $data, 'post');
}

function fetchUrl($url, $data, $type) {
  global $config;

  $ch = curl_init();
  curl_setopt($ch, CURLOPT_URL, $url);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
  curl_setopt($ch, CURLOPT_CONNECTTIMEOUT, 5);

  if (strtolower($type) == 'post') {
    curl_setopt($ch, CURLOPT_POST, TRUE);
  }

  if ($data) {
    $data_string = http_build_query($data);
    curl_setopt($ch, CURLOPT_POSTFIELDS, $data_string);
  }

  if (substr($url, 0, 5) == 'https') {
    curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, TRUE);
    curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, 2);
    curl_setopt($ch, CURLOPT_CAINFO, $config['net']['certfile']);
  }

  $result = curl_exec($ch);
  if ($result === FALSE && curl_errno($ch) == 60) {
    error_log("Failed to verify SSL certificate when loading $url. " .
              "Add the CA certificate to the CACerts folder");
  }
  error_log($result);
  $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);

  curl_close($ch);
  return $result !== FALSE && $httpCode == 200 ? $result : null;
}

// Take a path (relative or absolute) and generate a corresponding absolute URL
// This isn't perfect. Symlinks can trip it up but it should work for simple 
// cases.
//
// It works by comparing SCRIPT_FILENAME and SCRIPT_NAME to find how files map 
// to urls and then appends the base URL path to the given file.
function fileToUrl($file)
{
  // Get full path for $file
  $filePath = realpath($file);
  if (!$filePath)
    return false; // File not found

  // Get full path for script
  $scriptPath = realpath($_SERVER['SCRIPT_FILENAME']);
  if (!$scriptPath)
    return false; // Script not found

  // Find common part of paths
  $len = min(strlen($filePath), strlen($scriptPath));
  if ($len < 1)
    return false; // One of the paths is empty
  $i = 0;
  for (; $i < $len; $i++) {
    if ($filePath[$i] !== $scriptPath[$i])
      break;
  }
  if ($i == 0)
    return false; // Paths share nothing in common

  // Remove common part
  $filePath   = substr($filePath, $i);
  $scriptPath = substr($scriptPath, $i);

  // Convert directory separator to url syntax
  if (DIRECTORY_SEPARATOR != '/') {
    $filePath = strtr($filePath, DIRECTORY_SEPARATOR, '/');
    $scriptPath = strtr($scriptPath, DIRECTORY_SEPARATOR, '/');
  }

  // Check we're not going below the doc root
  if (strlen($scriptPath) > strlen($_SERVER['SCRIPT_NAME']))
    return false;

  // Remove non-common part of url
  if ($scriptPath) {
    $basePath = substr($_SERVER['SCRIPT_NAME'], 0, -strlen($scriptPath));
  } else {
    $basePath = $_SERVER['SCRIPT_NAME'];
  }

  // Append file path
  $path = $basePath . $filePath;

  // Make up url append the scheme, hostname, port, etc.
  $scheme      = @$_SERVER['HTTPS'] ? 'https://' : 'http://';
  $defaultPort = @$_SERVER['HTTPS'] ? '443' : '80';
  $port        = $_SERVER['SERVER_PORT'] != $defaultPort
               ? ':' . $_SERVER['SERVER_PORT']
               : '';
  $url         = $scheme . $_SERVER['SERVER_NAME'] . $port . $path;

  return $url;
}
