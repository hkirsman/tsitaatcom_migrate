<?php

/**
 * @file
 * Main module file for tsitaatcom_migrate.
 */

/*
 * @TODO: check timezone stuff for drupal and this import
 * @TODO: add est timezone for users
 * @TODO: MIKS NULL LÕPUS
 * http://drupal.tsitaat.com/content/raoul-wallenberg-0
 */

include_once 'tsitaatcom_migrate_utils.inc';

/*
 * Migrate tsitaat.com users.
 *
 * Last login is not imported. I think there isn't such a field in phpbb
 * and it's good not to import it to check later who has logged into new system.
 */
class TsitaatComUserMigration extends Migration {
  public function __construct() {

    parent::__construct();

    $connection = Database::getConnection('default', 'tsitaatcom');

    $query = $connection->select('phpbb_users', 'users');

    $query->fields('users', array(
        'user_id',
        'username_clean',
        'user_email',
        'user_regdate',
        'user_lastactivity'
      )
    )
    ->condition(db_or()
      ->condition('user_type', 0)
      ->condition('group_id', 5)
    );

    $this->source = new MigrateSourceSQL($query, array(), NULL, array('map_joinable' => FALSE));

    $this->destination = new MigrateDestinationUser();

    $this->map = new MigrateSQLMap($this->machineName,
      array('user_id' => array(
              'type' => 'int',
              'unsigned' => TRUE,
              'not null' => TRUE,
              ),
      ),
      MigrateDestinationUser::getKeySchema()
    );

    $this->addFieldMapping('uid', 'user_id');
    // To set custom uid.
    $this->addFieldMapping('is_new')->defaultValue(TRUE);
    $this->addFieldMapping('name', 'username_clean');
    $this->addFieldMapping('mail', 'user_email');
    $this->addFieldMapping('created', 'user_regdate');
    $this->addUnmigratedDestinations(array('theme', 'signature', 'access', 'login',
      'timezone', 'language', 'picture', 'signature_format', 'init'));
    // Is this correct?
    // $this->addFieldMapping('access', 'user_lastactivity');
    // $this->addUnmigratedDestinations(array('status'));
  }
}


/*
 * Migrate tsitaat.com quotes.
 */
class TsitaatComQuotesMigration extends Migration {

  /**
   * Implements __construct().
   */
  public function __construct() {

    $this->dependencies = array('TsitaatComUser');
    // TODO: should be uncommented, needed for test with 10 authors only
    // $this->dependencies = array('TsitaatComAuthors');

    parent::__construct();

    $connection = Database::getConnection('default', 'tsitaatcom');

    $query = $connection->select('a_tsitaatcom2_quotes', 'a_tsitaatcom2_quotes');

    // Get absolutely all quotes.
    $query
      ->fields('a_tsitaatcom2_quotes',
        array(
          'id',
          'q_id',
          'lang',
          'rank',
          'quote',
          'category',
          'zitate_de_category',
          'tag',
          'author_id',
          'author2_id',
          'source_id',
          'source_title',
          'source_title_extra_sufix',
          'source_title_extra_prefix',
          'source_year_start',
          'source_year_start_circa',
          'source_year_end',
          'created_by',
          'created_time',
          'revision',
          'active'
        )
      )
      ->orderBy('a_tsitaatcom2_quotes.id', 'DESC');


    $this->source = new MigrateSourceSQL($query, array(), NULL, array('map_joinable' => FALSE));

    $this->destination = new MigrateDestinationNode('quote');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'id' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
        ),
      ),
      MigrateDestinationNode::getKeySchema()
    );

    // Temporary, will set in prepareRow().
    $this->addFieldMapping('title', 'id');
    $this->addFieldMapping('nid', 'id');
    $this->addFieldMapping('is_new')->defaultValue(TRUE);
    $this->addFieldMapping('uid')->defaultValue(1);

    $this->addFieldMapping('field_id', 'id');
    $this->addFieldMapping('field_q_id', 'q_id');
    $this->addFieldMapping('field_lang', 'lang');
    $this->addFieldMapping('field_rank', 'rank');
    $this->addFieldMapping('field_quote', 'quote');
    $this->addFieldMapping('field_category', 'category');
    $this->addFieldMapping('field_zitate_de_category', 'zitate_de_category');
    $this->addFieldMapping('field_tag', 'tag');
    $this->addFieldMapping('field_author_id', 'author_id');
    $this->addFieldMapping('field_author2_id', 'author2_id');
    $this->addFieldMapping('field_source_id', 'source_id');
    $this->addFieldMapping('field_source_title', 'source_title');
    $this->addFieldMapping('field_source_title_extra_sufix', 'source_title_extra_sufix');
    $this->addFieldMapping('field_source_title_extra_prefix', 'source_title_extra_prefix');
    $this->addFieldMapping('field_source_year_start', 'source_year_start');
    $this->addFieldMapping('field_source_year_start_circa', 'source_year_start_circa');
    $this->addFieldMapping('field_source_year_end', 'source_year_end');
    $this->addFieldMapping('field_created_by', 'created_by');
    $this->addFieldMapping('field_created_time', 'created_time');
    $this->addFieldMapping('field_revision', 'revision');
    $this->addFieldMapping('field_active', 'active');
  }

  /**
   * Implements complete().
   */
  public function complete($node, $row) {
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

    $node->field_author_id_new['und'][0]['target_id'] = $author_nid;
    $node->field_created_by_new['und'][0]['target_id'] = $row->created_by;
    node_save($node);
  }
}

/*
 * Migrate tsitaat.com quote authors.
 */
class TsitaatComAuthorsMigration extends Migration {

  /**
   * Implements __construct().
   */
  public function __construct() {

    parent::__construct();

    $connection = Database::getConnection('default', 'tsitaatcom');

    $query = $connection->select('a_tsitaatcom2_quote_authors', 'authors');

    // Get absolutely all quotes.
    $query
      ->fields('authors', array(
                                  'id',
                                  'author_name_freeform_et',
                                  'author_name_freeform_en',
                                  'author_name_freeform_de',
                                  'author_name_amazon',
                                  'author_firstname_et',
                                  'author_firstname_en',
                                  'author_firstname_de',
                                  'author_midname_et',
                                  'author_midname_en',
                                  'author_midname_de',
                                  'author_lastname_et',
                                  'author_lastname_en',
                                  'author_lastname_de',
                                  'author_born_death_circa',
                                  'author_born',
                                  'author_born_circa',
                                  'author_death',
                                  'author_death_circa',
                                  'author_profession_et',
                                  'author_profession_en',
                                  'author_profession_de',
                                  'author_nationality',
                                  'is_tsitaatcom_user',
                                  'author_wiki_url_et',
                                  'author_wiki_url_en',
                                  'author_wiki_url_de',
                                  'url_friendly_author_name_et',
                                  'url_friendly_author_name_en',
                                  'url_friendly_author_name_de',
                                  'active',
          ))
      ->orderBy('authors.id', 'DESC');

    $this->source = new MigrateSourceSQL($query, array(), NULL, array('map_joinable' => FALSE));

    $this->destination = new MigrateDestinationNode('quote_author');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'id' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
        ),
      ),
      MigrateDestinationNode::getKeySchema()
    );

    // Temporary - will set in prepareRow().
    $this->addFieldMapping('title', 'title');
    $this->addFieldMapping('nid', 'id');
    $this->addFieldMapping('is_new')->defaultValue(TRUE);
    $this->addFieldMapping('uid')->defaultValue(1);

    $this->addFieldMapping('field_id', 'id');
    $this->addFieldMapping('field_author_name_freeform_et', 'author_name_freeform_et');
    $this->addFieldMapping('field_author_name_freeform_en', 'author_name_freeform_en');
    $this->addFieldMapping('field_author_name_freeform_de', 'author_name_freeform_de');
    $this->addFieldMapping('field_author_name_amazon', 'author_name_amazon');
    $this->addFieldMapping('field_author_firstname_et', 'author_firstname_et');
    $this->addFieldMapping('field_author_firstname_en', 'author_firstname_en');
    $this->addFieldMapping('field_author_firstname_de', 'author_firstname_de');
    $this->addFieldMapping('field_author_midname_et', 'author_midname_et');
    $this->addFieldMapping('field_author_midname_en', 'author_midname_en');
    $this->addFieldMapping('field_author_midname_de', 'author_midname_de');
    $this->addFieldMapping('field_author_lastname_et', 'author_lastname_et');
    $this->addFieldMapping('field_author_lastname_en', 'author_lastname_en');
    $this->addFieldMapping('field_author_lastname_de', 'author_lastname_de');
    $this->addFieldMapping('field_author_born_death_circa', 'author_born_death_circa');
    $this->addFieldMapping('field_author_born', 'author_born');
    $this->addFieldMapping('field_author_born_circa', 'author_born_circa');
    $this->addFieldMapping('field_author_death', 'author_death');
    $this->addFieldMapping('field_author_death_circa', 'author_death_circa');
    $this->addFieldMapping('field_author_profession_et', 'author_profession_et');
    $this->addFieldMapping('field_author_profession_en', 'author_profession_en');
    $this->addFieldMapping('field_author_profession_de', 'author_profession_de');
    $this->addFieldMapping('field_author_nationality', 'author_nationality');
    $this->addFieldMapping('field_is_tsitaatcom_user', 'is_tsitaatcom_user');
    $this->addFieldMapping('field_author_wiki_url_et', 'author_wiki_url_et');
    $this->addFieldMapping('field_author_wiki_url_en', 'author_wiki_url_en');
    $this->addFieldMapping('field_author_wiki_url_de', 'author_wiki_url_de');
    $this->addFieldMapping('field_urlfriendly_author_name_et', 'url_friendly_author_name_et');
    $this->addFieldMapping('field_urlfriendly_author_name_en', 'url_friendly_author_name_en');
    $this->addFieldMapping('field_urlfriendly_author_name_de', 'url_friendly_author_name_de');
    $this->addFieldMapping('field_active', 'active');
  }

  /**
   * Implements prepareRow().
   */
  public function prepareRow($current_row) {
    $author_title = _tsitaatcom_migrate_quote_author_title($current_row->author_name_freeform_et,
                                                             $current_row->author_firstname_et,
                                                             $current_row->author_midname_et,
                                                             $current_row->author_lastname_et);
    $current_row->title = $author_title;
    $current_row->temp_id = $current_row->id;
    $current_row->id = _tsitaatcom_migrate_get_new_node_id();
    foreach ($current_row as $key => $var) {
      // $current_row->$key = trim($var);
    }
  }

  /**
   * Implements complete().
   *
   * Change status and id.
   */
  public function complete($node, $row) {
    if ($row->active == 0) {
      $node->status = 0;
    }
    $node->field_id['und'][0]['value'] = $row->temp_id;
    $node->promote = 0;
    node_save($node);
  }
}
