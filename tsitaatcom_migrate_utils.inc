<?php

/**
 * @file
 * Tsitaatcom migrate utility functions.
 */

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
function _tsitaatcom_node_save($node) {
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
function _tsitaatcom_create_tags_and_set_relations($node, $vocabulary_name) {
  $vocabulary = taxonomy_vocabulary_machine_name_load($vocabulary_name);
  $tags = explode(",", $node->field_tag_old['und'][0]['value']);
  foreach($tags as $key => $tag) {
    $tag = trim($tag);
    $terms = taxonomy_get_term_by_name($tag, $vocabulary_name);
    if (count($terms)) {
      $term = array_pop($terms);
    } else {
      $term = new stdClass();
      $term->name = trim($tag);
      $term->vid = $vocabulary->vid;
      taxonomy_term_save($term);
    }
    $node->field_tag['und'][]['tid'] = $term->tid;
  }
  return $node;
}

/**
 * Set author relations.
 */
function _tsitaatcom_set_author_relations($node) {
  // Set author1 relations.
  {
    $result = db_query('SELECT nid
      FROM
        {node} n
      LEFT JOIN
        {field_data_field_id} field_author_id ON
        n.nid = field_author_id.entity_id
      WHERE
        field_id_value = :field_id_value
      AND
        TYPE = :type
      ',
      array(
        ':type' => 'quote_author',
        ':field_id_value' => $node->field_author_id['und'][0]['value'],
      )
    );

    foreach ($result as $item) {
      $author_nid = $item->nid;
    }


    $node->field_author['und'][0]['target_id'] = $author_nid;
  }

  // Set author2 relations.
  {
    $result = db_query('SELECT nid
      FROM
        {node} n
      LEFT JOIN
        {field_data_field_id} field_author_id ON
        n.nid = field_author_id.entity_id
      WHERE
        field_id_value = :field_id_value
      AND
        TYPE = :type
      ',
      array(
        ':type' => 'quote_author',
        ':field_id_value' => $node->field_author2_id['und'][0]['value'],
      )
    );

    $author2_nid = FALSE;
    foreach ($result as $item) {
      $author2_nid = $item->nid;
    }

    if ($author2_nid) {
      $node->field_author['und'][1]['target_id'] = $author2_nid;
    }
  }

  return $node;
}

/**
 * Set category relations.
 */
function _tsitaatcom_set_category_relations($node, $category_id_old) {
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
  $node->field_category['und'][0]['tid'] = $category_tid;
  return $node;
}
