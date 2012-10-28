<?php

/**
 * @file
 * Tsitaatcom migrate utility functions.
 */

 /**
 * Create nice author name.
 */
function _tsitaatcom_migrate_quote_author_title($author_name_freeform, $author_firstname, $author_midname, $author_lastname) {

  if (strlen(trim($author_name_freeform))) {
    return trim($author_name_freeform);
  }
  else {
    $name = trim($author_firstname);
    $name = trim($name . ' ' . $author_midname);
    $name = trim($name . ' ' . $author_lastname);
    return $name;
  }
}

/**
 * Finds biggest node id and increments 1.
 */
function _tsitaatcom_migrate_get_new_node_id() {
  $result = db_query('SELECT nid
  FROM
    {node} n
  ORDER BY nid DESC LIMIT 1');

  foreach ($result as $index) {
    $nid = $index->nid+1;
  }

  if (!isset($nid)) {
    $connection = Database::getConnection('default', 'tsitaatcom');
    $query = $connection->select('a_tsitaatcom2_quotes', 'a_tsitaatcom2_quotes');
    $query
      ->fields('a_tsitaatcom2_quotes', array('id'))
      ->orderBy('a_tsitaatcom2_quotes.id', 'DESC')
      ->range(0,1);

    $result = $query->execute();

    foreach ($result as $index) {
      $nid = $index->id+1;
    }
  }

  return $nid;
}


function _tsitaatcom_migrate_get_uid($user_name) {
  print_r($user_name);
}
