<?php

/**
 * @file
 * Tsitaatcom migrate utility functions.
 */

define('TSITAATCOM_MIGRATE_UID_HANNES', 2);
define('TSITAATCOM_MIGRATE_ANONYMOUS_IMAGE', 'noauthorimg.jpg');


 /**
  * Create nice author name.
  */
function _tsitaatcom_migrate_quote_author_title($author_name_freeform, $author_firstname, $author_midname, $author_lastname) {

  if (drupal_strlen(trim($author_name_freeform))) {
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
  $result = db_query('
    SELECT nid
    FROM
      {node} n
    ORDER BY nid DESC LIMIT 1'
  );

  foreach ($result as $index) {
    $nid = $index->nid + 1;
  }

  if (!isset($nid)) {
    $connection = Database::getConnection('default', 'tsitaatcom');
    $query = $connection->select('a_tsitaatcom2_quotes', 'a_tsitaatcom2_quotes');
    $query
      ->fields('a_tsitaatcom2_quotes', array('id'))
      ->orderBy('a_tsitaatcom2_quotes.id', 'DESC')
      ->range(0, 1);

    $result = $query->execute();

    foreach ($result as $index) {
      $nid = $index->id + 1;
    }
  }

  return $nid;
}

/**
 * Turns '2011-12-07 21:20:21' like fields into timestamp.
 */
function _tsitaatcom_mysql_datefield_to_timestamp($datefield) {
  // Year.
  $Y = substr($datefield, 0, 4);
  // Month.
  $n = substr($datefield, 5, 2);
  // Day.
  $j = substr($datefield, 8, 2);
  // Hour.
  $H = substr($datefield, 11, 2);
  // Minue.
  $i = substr($datefield, 14, 2);
  // Second.
  $s = substr($datefield, 17, 2);
  return mktime($H, $i, $s, $n, $j, $Y);
}

/**
 * Return uid based on username.
 */
function _tsitaatcom_migrate_get_uid($user_name) {
  $result = db_query('
    SELECT
      uid
    FROM
      users
    WHERE name =  :name',
    array(':name' => 'hannes')
  );

  foreach ($result as $record) {
    return $record->uid;
  }

  return FALSE;
}

/**
 * Hack node_save() so the changed field equals created field.
 */
function _tsitaatcom_migrate_node_save($node) {
  $transaction = db_transaction();

  try {
    // Load the stored entity, if any.
    if (!empty($node->nid) && !isset($node->original)) {
      $node->original = entity_load_unchanged('node', $node->nid);
    }

    field_attach_presave('node', $node);
    global $user;

    // Determine if we will be inserting a new node.
    if (!isset($node->is_new)) {
      $node->is_new = empty($node->nid);
    }

    // Set the timestamp fields.
    if (empty($node->created)) {
      $node->created = REQUEST_TIME;
    }
    // The changed timestamp is always updated for bookkeeping purposes,
    // for example: revisions, searching, etc.
    $node->changed = $node->created;

    $node->timestamp = REQUEST_TIME;
    $update_node = TRUE;

    // Let modules modify the node before it is saved to the database.
    module_invoke_all('node_presave', $node);
    module_invoke_all('entity_presave', $node, 'node');

    if ($node->is_new || !empty($node->revision)) {
      // When inserting either a new node or a new node revision, $node->log
      // must be set because {node_revision}.log is a text column and therefore
      // cannot have a default value. However, it might not be set at this
      // point (for example, if the user submitting a node form does not have
      // permission to create revisions), so we ensure that it is at least an
      // empty string in that case.
      // @todo: Make the {node_revision}.log column nullable so that we can
      // remove this check.
      if (!isset($node->log)) {
        $node->log = '';
      }
    }
    elseif (!isset($node->log) || $node->log === '') {
      // If we are updating an existing node without adding a new revision, we
      // need to make sure $node->log is unset whenever it is empty. As long as
      // $node->log is unset, drupal_write_record() will not attempt to update
      // the existing database column when re-saving the revision; therefore,
      // this code allows us to avoid clobbering an existing log entry with an
      // empty one.
      unset($node->log);
    }

    // When saving a new node revision, unset any existing $node->vid so as to
    // ensure that a new revision will actually be created, then store the old
    // revision ID in a separate property for use by node hook implementations.
    if (!$node->is_new && !empty($node->revision) && $node->vid) {
      $node->old_vid = $node->vid;
      unset($node->vid);
    }

    // Save the node and node revision.
    if ($node->is_new) {
      // For new nodes, save new records for both the node itself and the node
      // revision.
      drupal_write_record('node', $node);
      _node_save_revision($node, $user->uid);
      $op = 'insert';
    }
    else {
      // For existing nodes, update the node record which matches the value of
      // $node->nid.
      drupal_write_record('node', $node, 'nid');
      // Then, if a new node revision was requested, save a new record for
      // that; otherwise, update the node revision record which matches the
      // value of $node->vid.
      if (!empty($node->revision)) {
        _node_save_revision($node, $user->uid);
      }
      else {
        _node_save_revision($node, $user->uid, 'vid');
        $update_node = FALSE;
      }
      $op = 'update';
    }
    if ($update_node) {
      db_update('node')
        ->fields(array('vid' => $node->vid))
        ->condition('nid', $node->nid)
        ->execute();
    }

    // Call the node specific callback (if any). This can be
    // node_invoke($node, 'insert') or
    // node_invoke($node, 'update').
    node_invoke($node, $op);

    // Save fields.
    $function = "field_attach_$op";
    $function('node', $node);

    module_invoke_all('node_' . $op, $node);
    module_invoke_all('entity_' . $op, $node, 'node');

    // Update the node access table for this node. There's no need to delete
    // existing records if the node is new.
    $delete = $op == 'update';
    node_access_acquire_grants($node, $delete);

    // Clear internal properties.
    unset($node->is_new);
    unset($node->original);
    // Clear the static loading cache.
    entity_get_controller('node')->resetCache(array($node->nid));

    // Ignore slave server temporarily to give time for the
    // saved node to be propagated to the slave.
    db_ignore_slave();
  }
  catch (Exception $e) {
    $transaction->rollback();
    watchdog_exception('node', $e);
    throw $e;
  }
}

/**
 * Create and set tags.
 */
function _tsitaatcom_migrate_create_tags_and_set_relations($node, $row, $vocabulary_name) {
  $vocabulary = taxonomy_vocabulary_machine_name_load($vocabulary_name);
  $tags = explode(",", $row->tag);
  foreach($tags as $key => $tag) {
    $tag = trim($tag);
    if (drupal_strlen($tag) === 0) {
      continue;
    }
    $terms = taxonomy_get_term_by_name($tag, $vocabulary_name);
    if (count($terms)) {
      $term = array_pop($terms);
    } else {
      $term = new stdClass();
      $term->name = trim($tag);
      $term->vid = $vocabulary->vid;
      taxonomy_term_save($term);
    }
    $node->field_q_tag[LANGUAGE_NONE][]['tid'] = $term->tid;
  }
  return $node;
}

/**
 * Set author relations.
 */
function _tsitaatcom_migrate_set_author_relations($node) {
  // Set author1 relations.
  {
    $result = db_query('
      SELECT nid
      FROM
        {node} n
      LEFT JOIN
        {field_data_field_q_author_id_old} author_id_old ON
        n.nid = author_id_old.entity_id
      WHERE
        field_q_author_id_old_value = :author_id_old
      AND
        type = :type
     ',
      array(
        ':type' => 'quote',
        ':author_id_old' => $node->field_id[LANGUAGE_NONE][0]['value'],
      )
    );

    foreach ($result as $item) {
      $node_quote = node_load($item->nid);
      $node_quote->field_q_author[LANGUAGE_NONE][0]['target_id'] = $node->nid;
      field_attach_update('node', $node_quote);

      $node_quote->path['source'] = 'node/' . $node_quote->nid;
      if (drupal_strlen(trim($node->field_urlfriendly_author_name[LANGUAGE_NONE][0]['value'])) === 0) {
        dpm('Warning: ' . $node_quote->nid . ' has empty field_urlfriendly_author_name value.');
      }
      $node_quote->path['alias'] = 'tsitaadid/autorid/' . $node->field_urlfriendly_author_name[LANGUAGE_NONE][0]['value'] . '/' . $node_quote->nid;
      $node_quote->path['language'] = LANGUAGE_NONE;
      path_save($node_quote->path);
    }
  }

  // Set author2 relations.
  {
    $result = db_query('
      SELECT nid
      FROM
        {node} n
      LEFT JOIN
        {field_data_field_q_author2_id_old} author2_id_old ON
        n.nid = author2_id_old.entity_id
      WHERE
        field_q_author2_id_old_value = :author2_id_old
      AND
        type = :type
     ',
      array(
        ':type' => 'quote',
        ':author2_id_old' => $node->field_id[LANGUAGE_NONE][0]['value'],
      )
    );

    foreach ($result as $item) {
      $node_quote = node_load($item->nid);
      $node_quote->field_q_author[LANGUAGE_NONE][1]['target_id'] = $node->nid;
      field_attach_update('node', $node_quote);
    }
  }
}

/**
 * Set quote source relations.
 */
function _tsitaatcom_migrate_set_quote_source_relations($node) {
  $result = db_query('
    SELECT nid
    FROM
      {node} n
    LEFT JOIN
      {field_data_field_q_source_id_old} source_id_old ON
      n.nid = source_id_old.entity_id
    WHERE
      field_q_source_id_old_value = :source_id_old
    AND
      type = :type
   ',
    array(
      ':type' => 'quote',
      ':source_id_old' => $node->field_qs_id[LANGUAGE_NONE][0]['value'],
    )
  );

  foreach ($result as $item) {
    $node_quote = node_load($item->nid);
    $node_quote->field_q_source[LANGUAGE_NONE][0]['target_id'] = $node->nid;
    field_attach_update('node', $node_quote);
  }
}

/**
 * Set category relations.
 */
function _tsitaatcom_migrate_set_category_relations($node, $category_id_old) {
  $result = db_query('SELECT tid
    FROM
      {taxonomy_term_data} n
    LEFT JOIN
      {field_data_field_id} field_id ON
      n.tid = field_id.entity_id
    WHERE
      field_id_value = :field_id_value
    ',
    array(
      ':field_id_value' => $category_id_old,
    )
  );

  foreach ($result as $item) {
    $category_tid = $item->tid;
  }

  // Set category relations.
  $node->field_q_category[LANGUAGE_NONE][0]['tid'] = $category_tid;
  return $node;
}

/**
 * Create author portrait node, attach image to it and set relation in author node.
 */
function _tsitaatcom_migrate_attach_author_portrait($node) {
  $author_id_old = $node->field_id[LANGUAGE_NONE][0]['value'];

  $connection = Database::getConnection('default', 'tsitaatcom');
  $query = $connection->select('a_tsitaatcom2_images', 'i');
  $query
    ->fields('i', array('id'))
    ->condition('value', $author_id_old, '=')
    ->condition('name', 'author_id', '=');

  $result = $query->execute();

  foreach ($result as $index) {
    $id = $index->id;
  }

  if (isset($id)) {
    $connection2 = Database::getConnection('default', 'tsitaatcom');
    $query2 = $connection2->select('a_tsitaatcom2_images', 'i');
    $query2
      ->fields('i', array('name', 'value'))
      ->condition('id', $id, '=');

    $result = $query2->execute();

    while($record = $result->fetchAssoc()) {
      if ($record['name'] == 'author_id') {
        $author_id = $record['value'];
      }
      if ($record['name'] == 'nice_name') {
        $nice_name = $record['value'];
      }
      if ($record['name'] == 'url') {
        $url = $record['value'];
      }
    }

    $nid_author_portrait = _tsitaatcom_migrate_get_nid_by_node_title($nice_name);
    if($nid_author_portrait === FALSE) {
      $nid_author_portrait = _tsitaatcom_migrate_create_node_quote_author_portrait($nice_name, '/var/www/tsitaat/__files/' . $nice_name);
    }
  } else {
    $nid_author_portrait = _tsitaatcom_migrate_get_nid_by_node_title(TSITAATCOM_MIGRATE_ANONYMOUS_IMAGE);

    if($nid_author_portrait === FALSE) {
      $nid_author_portrait = _tsitaatcom_migrate_create_node_quote_author_portrait(TSITAATCOM_MIGRATE_ANONYMOUS_IMAGE, '/var/www/tsitaat/__files/noauthorimg.jpg');
    }
  }

  // Create reference to the new author portrait node.
  $node->field_author_portrait_ref[LANGUAGE_NONE][0]['target_id'] = $nid_author_portrait;
  $node->field_author_portrait_ref[LANGUAGE_NONE][0]['target_id'];
  field_attach_update('node', $node);
}

/**
 * Create node type quote_author_portrait and attach image.
 *
 */
function _tsitaatcom_migrate_create_node_quote_author_portrait($name, $filepath) {
  $node_quote_author_portrait = new stdClass();
  $node_quote_author_portrait->title = $name;
  $node_quote_author_portrait->type = 'quote_author_portrait';
  node_object_prepare($node_quote_author_portrait); // Sets some defaults. Invokes hook_prepare() and hook_node_prepare().
  $node_quote_author_portrait->language = LANGUAGE_NONE; // Or e.g. 'en' if locale is enabled
  $node_quote_author_portrait->uid = TSITAATCOM_MIGRATE_UID_HANNES;
  $node_quote_author_portrait->status = 1; // (1 or 0): published or not

  // Create managed File object and associate with Image field.
  $file = (object) array(
    'uid' => TSITAATCOM_MIGRATE_UID_HANNES,
    'uri' => $filepath,
    'filemime' => file_get_mimetype($filepath),
    'status' => 1,
  );

  // We save the file to the root of the files directory.
  $file = file_copy($file, 'public://');

  $node_quote_author_portrait->field_author_portrait[LANGUAGE_NONE][0] = (array)$file;

  node_save($node_quote_author_portrait);

  _tsitaatcom_watchdog_author_portrait($name);

  return $node_quote_author_portrait->nid;
}

/**
 * Check if node by the defined name exists.
 *
 */
function _tsitaatcom_migrate_get_nid_by_node_title($node_title) {
  $nid = db_query('SELECT nid FROM node WHERE title = :title', array(
      ':title' => $node_title))->fetchField();

  if(isset($nid)) {
    return $nid;
  } else {
    return FALSE;
  }
}

/**
 * Set translations for quote.
 * Help from https://drupal.org/node/1069774
 * @param  object $node Node object
 * @param  object $row  Row object from the old database
 * @return void
 */
function _tsitaatcom_migrate_set_translations($node, $row) {
  db_set_active('tsitaatcom');
  $results = db_query('
    SELECT
      lang,
      quote
    FROM
      a_tsitaatcom2_quotes
    WHERE
      q_id = :quote_id
    AND
      lang != \'et\';
    ', array(
      ':quote_id' => $row->id
    ))->fetchAll(PDO::FETCH_ASSOC);
  db_set_active();

  $translations = array();
  foreach($results as $result) {
    $translation = array(
      'translate' => 0,
      'status' => 1,
      'language' => $result['lang'], // here is the language you're translating to
      'source' => 'et', // here is the source language
    );
    $handler = entity_translation_get_handler('node', $node);
    $values['field_q_quote'][$result['lang']]['0']['value'] = $result['quote'];
    $handler->setTranslation($translation, $values);
    field_attach_update('node', $node);

    $translations[$result['lang']] = $result['lang'];
  }

  $node->field_q_language_auto[LANGUAGE_NONE][0]['value'] = serialize($translations);
  field_attach_update('node', $node);
}
