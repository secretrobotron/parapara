<?php
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

require_once("parapara.inc");
require_once("db.inc");
require_once("exceptions.inc");
require_once("UriUtils.inc");

// When updating this, be sure to update wall.js as well
define("WALLMAKER_SESSION_NAME", "WMSESSID");

// Given a user id, returns an array of walls owned by that user with the 
// following information:
//
//   wallId
//   event name
//   design thumb URL
//   whether they will display in the gallery or not
//   creation date (UTC)
//   modification date (UTC)
//
// The results will be sorted by creation date such that recently created walls 
// sort before older walls.

function getWallSummaryForUser($email) {
  // XXX Convert this to use exceptions
  // Get connection
  global $config;
  $mdb2 =& MDB2::factory($config['db']['dsn']);
  if (PEAR::isError($mdb2)) {
    error_log('Error connecting to DB: ' . $mdb2->getMessage()
               . ', ' . $mdb2->getDebugInfo());
    return null;
  }

  // Run query
  $res =& $mdb2->query(
      'SELECT walls.wallId, walls.eventName, designs.thumbUrl,'
    . ' walls.galleryDisplay, walls.createDate, walls.modifyDate'
    . ' FROM users, walls'
    . ' LEFT JOIN designs on designs.designId = walls.designId'
    . ' WHERE users.email = '
    . $mdb2->quote($email, 'text')
    . '   AND walls.owner = users.userId'
    . ' ORDER BY walls.createDate DESC');
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    return null;
  }

  // Process each array so we can tweak the values and make them a little less 
  // bound to the db schema
  $result = array();
  $mdb2->setFetchMode(MDB2_FETCHMODE_ASSOC);
  while ($row = $res->fetchRow()) {
    $result[] =
      array(
        'wallId' => $row['wallid'],
        'eventName' => $row['eventname'],
        'thumb' => $row['thumburl'],
        'galleryDisplay' => !!($row['gallerydisplay']),
        'createDate' => $row['createdate'],
        'modifyDate' => $row['modifydate']
      );
  }

  // Tidy up and finish
  $mdb2->disconnect();

  return $result;
}

function createWall($params) {
  // Get connection
  $conn =& getDbConnection();

  // Validate owner email
  $email = trim(@$params['ownerEmail']);
  if (!strlen($email) || !filter_var($email, FILTER_VALIDATE_EMAIL)) {
    error_log("Bad or missing email when creating wall");
    throw new KeyedException('server-error', 'No email for user');
  }

  // Fetch user ID for owner
  $ownerId = getOrCreateUserIdFromEmail($email);

  // Validate title
  $title = trim(@$params['title']);
  if (!strlen($title)) {
    error_log("Empty title");
    throw new KeyedException('empty-title');
  }
  // XXX Test this
  if (!isTitleUnique($title)) {
    error_log("Even with title '$title' already exists.");
    throw new KeyedException('duplicate-title');
  }

  // Validate ID
  $designId = intval(@$params['design']);
  if ($designId < 1) {
    error_log("Bad design ID: $designId");
    throw new KeyedException('design-not-found', 'Bad design ID');
  }
  if (!isDesignIdValid($designId)) {
    error_log("Could't find design ID " . $designId);
    throw new KeyedException('design-not-found', 'ID not found');
  }

  // Get URLs
  $path      = getPathForTitle($title);
  $wallUrl   = getWallUrlForPath($path);
  $editorUrl = getWallUrlForPath($path);

  $shortUrl = shortenUrl($wallUrl);
  if ($shortUrl == $wallUrl)
    $shortUrl = NULL;

  $editorShortUrl = shortenUrl($editorUrl);
  if ($editorShortUrl == $editorUrl)
    $editorShortUrl = NULL;

  $date = gmdate('Y-m-d H:i:s');

  // Run query
  $query =
    'INSERT INTO walls'
    . ' (owner, designId, eventName, galleryDisplay, urlPath, shortUrl,'
    . '  editorShortUrl, createDate, modifyDate)'
    . ' VALUES '
    . '(' . $conn->quote($ownerId, 'integer')
    . ',' . $conn->quote($designId, 'integer')
    . ',' . $conn->quote($title, 'text')
    . ',TRUE' // galleryDisplay
    . ',' . $conn->quote($path, 'text')
    . ',' . (($shortUrl) ? $conn->quote($shortUrl, 'text') : 'NULL')
    . ',' . (($editorShortUrl) ? $conn->quote($editorShortUrl, 'text') : 'NULL')
    . ',' . $conn->quote($date, 'timestamp')
    . ',' . $conn->quote($date, 'timestamp')
    . ')';
  $res =& $conn->query($query);
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }

  // Get ID of newly created wall
  $wallId = $conn->lastInsertID('walls', 'wallId');
  if (PEAR::isError($wallId)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }

  return intval($wallId);
}

function getOrCreateUserIdFromEmail($email) {
  $conn =& getDbConnection();
  $res =& $conn->queryOne(
      'SELECT userId FROM users WHERE email = '
    . $conn->quote($email, 'text')
    . ' LIMIT 1',
    'integer');
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }

  // If user does not exist, add a record for them
  if ($res >= 1)
    return $res;

  $res =& $conn->query('INSERT INTO users (email) VALUES ('
    . $conn->quote($email, 'text')
    . ')');
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }

  $ownerId = $conn->lastInsertID('users', 'userId');
  if (PEAR::isError($ownerId)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }

  return $ownerId;
}

function getWall($wallId) {
  $conn =& getDbConnection();
  $res =& $conn->query(
      'SELECT walls.designId,eventName,eventDescr,eventLocation,eventType,eventFinish,urlPath,shortUrl,editorShortUrl,walls.duration,galleryDisplay,passcode,designs.duration AS defaultduration FROM walls,designs WHERE walls.designId = designs.designId AND wallId = '
    . $conn->quote($wallId, 'integer')
  );
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  $result = array();
  $conn->setFetchMode(MDB2_FETCHMODE_ASSOC);
  if ($row = $res->fetchRow()) {
    $result["designId"] = $row['designid'];
    $result["eventName"] = $row['eventname'];
    $result["eventDescr"] = $row['eventdescr'];
    $result["eventLocation"] = $row['eventlocation'];
    $result["eventType"] = $row['eventtype'];
    $result["eventFinish"] = $row['eventfinish'];
    $result["urlPath"] = $row['urlpath'];
    $result["shortUrl"] = $row['shorturl'];
    $result["editorShortUrl"] = $row['editorshorturl'];
    $result["galleryDisplay"] = $row['gallerydisplay'];
    $result["duration"] = $row['duration'];
    $result["defaultDuration"] = $row['defaultduration'];
    $result["passcode"] = $row['passcode'] != NULL ? strlen($row['passcode']) : 0;
  }
  $conn->disconnect();
  return $result;
}

function updateWall($wallId, $name, $value, $type) {
  $conn =& getDbConnection();
  $qvalue = $conn->quote($value, $type);
  $qwallid = $conn->quote($wallId, 'integer');
  $query = 'UPDATE walls SET '.$name.'='.$qvalue.' WHERE wallId = '.$qwallid;
  $res =& $conn->query($query);
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  $conn->disconnect();
}

// Returns NULL if the wall is not found
function getWallIdFromPath($path) {
  $conn =& getDbConnection();
  $res =& $conn->queryOne(
      'SELECT wallId FROM walls WHERE urlPath = '
      . $conn->quote(rawurlencode($path), 'text')
      . ' LIMIT 1',
      'integer');
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  return $res;
}

function startNewSession($wallId, $datetime) {
  $conn =& getDbConnection();
  $qwallid = $conn->quote($wallId, 'integer');
  $qdatetime = $conn->quote($datetime, 'timestamp');
  $query = 'INSERT INTO sessions(wallId,beginDate) VALUES('.$wallId.','.$qdatetime.')';
  $res =& $conn->query($query);
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  $conn->disconnect();
}

function closeLastSession($wallId, $datetime) {
  $conn =& getDbConnection();
  $qwallid = $conn->quote($wallId, 'integer');
  $qdatetime = $conn->quote($datetime, 'timestamp');
  //makes temp table for (You can't specify target table 'sessions' for update in FROM clause)
  $query = 'UPDATE sessions SET endDate='.$qdatetime.' WHERE endDate IS NULL AND wallId='.$wallId.' AND sessionId=(SELECT temp.sessionId FROM (SELECT MAX(sessionId) AS sessionID FROM sessions WHERE wallId='.$wallId.') AS temp)';
  $res =& $conn->query($query);
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  $conn->disconnect();
}

// Returns NULL if there is no active session for the wall
function getActiveSession($wallId) {
  $conn =& getDbConnection();
  $date = gmdate('Y-m-d H:i:s');
  $qdate = $conn->quote($date, 'timestamp');

  $res =& $conn->queryOne(
      'SELECT sessionId FROM sessions WHERE'
      . ' wallId = ' . $conn->quote($wallId, 'integer')
      . ' AND beginDate < ' . $qdate
      . ' AND (endDate IS NULL OR endDate > ' . $qdate . ')',
      'integer');
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  return $res;
}

function isTitleUnique($title) {
  return !recordExists('walls', 'eventName', $title, 'text');
}

function isDesignIdValid($id) {
  return recordExists('designs', 'designId', $id, 'integer');
}

function getWallUrlForPath($path) {
  $scheme = 'http://';
  $port   = intval($_SERVER['SERVER_PORT']) !== 80
          ? ':' . $_SERVER['SERVER_PORT']
          : '';
  $path   = '/wall/' . $path;
  $url    = $scheme . $_SERVER['SERVER_NAME'] . $port . $path;
  return $url;
}

function getEditorUrlForPath($path) {
  global $config;
  return $config['editor']['url'] . $path;
}

function getPathForTitle($title) {
  // Strip leading/trailing space, and
  // convert fullwidth forms to halfwidth
  $path = mb_convert_kana(trim($title), "as", "utf-8");
  // Make lowercase, and
  // convert whitespace to -
  $path = strtr(strtolower($path), " \t", '--');
  // URL encode
  $path = rawurlencode($path);
  // Check for duplicates
  // If we have a clash, just generate a random ID and use that--the user can 
  // make it something more sensible later if they wish
  if (!isPathUnique($path))
    $path = rawurlencode(uniqid());
  return $path;
}

// Input: a URL-encoded path
function isPathUnique($path) {
  return !recordExists('walls', 'urlPath', $path, 'text');
}

function recordExists($table, $field, $value, $type) {
  $conn =& getDbConnection();
  $res =& $conn->query(
      "SELECT * FROM $table WHERE $field = "
      . $conn->quote($value, $type)
      . " LIMIT 1");
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  $exists = $res->numRows() > 0;
  $res->free();
  return $exists;
}

function getCurrentWallTimeByWallId($wallId) {
  $conn =& getDbConnection();
  $res =& $conn->query(
      'SELECT walls.duration AS duration,designs.duration AS defaultduration FROM walls,designs WHERE walls.designId = designs.designId AND wallId = '
    . $conn->quote($wallId, 'integer')
  );
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  $conn->setFetchMode(MDB2_FETCHMODE_ASSOC);
  $currentDuration = 0;
  if ($row = $res->fetchRow()) {
    $duration = intval($row['duration']);
    $defaultDuration = intval($row['defaultduration']);
    $currentDuration = $duration != 0 ? $duration : $defaultDuration;
  }
  $conn->disconnect();
  return getCurrentWallTimeByDuration($currentDuration);
}

function getCurrentWallDuration($wallId) {
  $conn =& getDbConnection();
  $res =& $conn->query(
      'SELECT walls.duration AS duration,designs.duration AS defaultduration FROM walls,designs WHERE walls.designId = designs.designId AND wallId = '
    . $conn->quote($wallId, 'integer')
  );
  if (PEAR::isError($res)) {
    error_log($res->getMessage() . ', ' . $res->getDebugInfo());
    throw new KeyedException('db-error');
  }
  $conn->setFetchMode(MDB2_FETCHMODE_ASSOC);
  $currentDuration = 0;
  if ($row = $res->fetchRow()) {
    $duration = intval($row['duration']);
    $defaultDuration = intval($row['defaultduration']);
    $currentDuration = $duration != 0 ? $duration : $defaultDuration;
  }
  $conn->disconnect();
  return $currentDuration;
}

function getCurrentWallTimeByDuration($duration) {
  $timeparts = explode(" ",microtime());
  $currentTimeMillis = bcadd(($timeparts[0]*1000),bcmul($timeparts[1],1000));
  return $currentTimeMillis % $duration;
}
?>
