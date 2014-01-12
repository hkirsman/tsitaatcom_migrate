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
// class TsitaatComUserMigration extends Migration {
//   /**
//    * Implements __construct().
//    */
//   public function __construct() {

//     parent::__construct();

//     $connection = Database::getConnection('default', 'tsitaatcom');

//     $query = $connection->select('phpbb_users', 'users');

//     $query->fields(
//       'users',
//       array(
//         'user_id',
//         'username_clean',
//         'user_email',
//         'user_regdate',
//         'user_lastactivity',
//       )
//     )
//     ->condition(db_or()
//       ->condition('user_type', 0)
//       ->condition('group_id', 5)
//     );

//     $this->source = new MigrateSourceSQL($query, array(), NULL, array('map_joinable' => FALSE));

//     $this->destination = new MigrateDestinationUser();

//     $this->map = new MigrateSQLMap(
//       $this->machineName,
//       array(
//         'user_id' => array(
//           'type' => 'int',
//           'unsigned' => TRUE,
//           'not null' => TRUE,
//         ),
//       ),
//       MigrateDestinationUser::getKeySchema()
//     );

//     $this->addFieldMapping('uid', 'user_id');
//     // To set custom uid.
//     $this->addFieldMapping('is_new')->defaultValue(TRUE);
//     $this->addFieldMapping('name', 'username_clean');
//     $this->addFieldMapping('mail', 'user_email');
//     $this->addFieldMapping('created', 'user_regdate');
//     $this->addUnmigratedDestinations(array('theme', 'signature', 'access', 'login',
//       'timezone', 'language', 'picture', 'signature_format', 'init'));
//     // Is this correct?
//     // $this->addFieldMapping('access', 'user_lastactivity');
//     // $this->addUnmigratedDestinations(array('status'));
//   }
// }

/*
 * Migrate tsitaat.com categories.
 */
class TsitaatComQuoteCategoriesMigration extends Migration {

  /**
   * Implements __construct().
   */
  public function __construct() {

    parent::__construct();

    $connection = Database::getConnection('default', 'tsitaatcom');

    $query = $connection->select('a_tsitaatcom2_categories', 'a_tsitaatcom2_categories');

    // Get absolutely all quotes.
    $query
      ->fields(
        'a_tsitaatcom2_categories',
        array(
          'id',
          'name_et',
          'name_en',
        )
      )
      ->orderBy('a_tsitaatcom2_categories.id', 'DESC');


    $this->source = new MigrateSourceSQL($query, array(), NULL, array('map_joinable' => FALSE));

    $this->destination = new MigrateDestinationTerm('quote_category');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'id' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
        ),
      ),
      MigrateDestinationTerm::getKeySchema()
    );

    // Temporary, will set in prepareRow().
    $this->addFieldMapping('name', 'name_et');
    $this->addFieldMapping('is_new')->defaultValue(FALSE);
    $this->addFieldMapping('uid')->defaultValue(1);
    $this->addFieldMapping('language')->defaultValue('et');

    $this->addFieldMapping('field_id', 'id');
    $this->addFieldMapping('field_name_et', 'name_et');
    $this->addFieldMapping('field_name_en', 'name_en');
  }

  /**
   * Implements complete().
   */
  public function complete($node, $row) {

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

    $this->dependencies = array('TsitaatComQuoteCategories');

    parent::__construct();

    $connection = Database::getConnection('default', 'tsitaatcom');

    $query = $connection->select('a_tsitaatcom2_quotes', 'a_tsitaatcom2_quotes');

    // Get estonian quotes.
    $query
      ->fields(
        'a_tsitaatcom2_quotes',
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
          'source_title_extra_prefix',
          'source_title_extra_sufix',
          'source_year_start',
          'source_year_start_circa',
          'source_year_end',
          'created_by',
          'created_time',
          'revision',
          'active',
        )
      )
      ->condition('a_tsitaatcom2_quotes.lang', 'et', '=')
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
    $this->addFieldMapping('language')->defaultValue('et');
    $this->addFieldMapping('status', 'active');

     // Probably don't need to define field_q_language_auto here.
    $this->addFieldMapping('field_q_language_auto')->defaultValue(serialize(array()));
    $this->addFieldMapping('field_q_rank', 'rank');
    $this->addFieldMapping('field_q_quote', 'quote');
    $this->addFieldMapping('field_q_source_title_xtra_prefix', 'source_title_extra_prefix');
    $this->addFieldMapping('field_q_source_title_xtra_suffix', 'source_title_extra_sufix');
    // Fields needed for migration.
    $this->addFieldMapping('field_q_author_id_old', 'author_id');
    $this->addFieldMapping('field_q_author2_id_old', 'author2_id');
    $this->addFieldMapping('field_q_source_id_old', 'source_id');
  }

  public function prepare($node, $row) {
    _tsitaatcom_row_cleanup($node);
  }

  /**
   * Implements complete().
   *
   * Set quote author relationship, change created and changed fields.
   * Todo. Get second author relationsip.
   */
  public function complete($node, $row) {
    // Set tag auto field.
    $tags = explode(',', trim($row->tag));
    $last_tags_key = end(array_keys($tags));
    $node->field_q_tag_auto[LANGUAGE_NONE][0]['value'] = '';
    foreach($tags as $key => $tag) {
      if ($key == $last_tags_key) {
        $node->field_q_tag_auto[LANGUAGE_NONE][0]['value'] .= trim($tag);
      } else {
        $node->field_q_tag_auto[LANGUAGE_NONE][0]['value'] .= trim($tag) . ', ';
      }
    }
    // Set translations.
    _tsitaatcom_migrate_set_translations($node, $row);
    // Set category relations.
    _tsitaatcom_migrate_set_category_relations($node, $row->category);
    // Create and set tags.
    $node = _tsitaatcom_migrate_create_tags_and_set_relations($node, $row, 'quote_tag');
    // Set correct uid.
    $node->uid = $row->created_by;
    // Set creation date.
    $node->created = _tsitaatcom_mysql_datefield_to_timestamp($row->created_time);
    // Set default promote to 0.
    $node->promote = 0;
    // Hack node_save() so it changes 'changed' field equal with created.
    _tsitaatcom_migrate_node_save($node);
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

    //$this->dependencies = array('TsitaatComQuotes');

    parent::__construct();

    $connection = Database::getConnection('default', 'tsitaatcom');

    $query = $connection->select('a_tsitaatcom2_quote_authors', 'authors');

    // Get absolutely all quotes.
    $query
      ->fields(
        'authors',
        array(
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
        )
      )
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

    // Temporary name for title - will set in prepareRow().
    $this->addFieldMapping('title', 'title');
    $this->addFieldMapping('is_new')->defaultValue(TRUE);
    $this->addFieldMapping('uid')->defaultValue(1);
    $this->addFieldMapping('language')->defaultValue('et');
    $this->addFieldMapping('status', 'active');
    $this->addFieldMapping('promote')->defaultValue(0);

    $this->addFieldMapping('field_qa_name_freeform', 'author_name_freeform_et');
    $this->addFieldMapping('field_qa_firstname', 'author_firstname_et');
    $this->addFieldMapping('field_qa_midname', 'author_midname_et');
    $this->addFieldMapping('field_qa_lastname', 'author_lastname_et');
    $this->addFieldMapping('field_qa_born_year', 'author_born');
    $this->addFieldMapping('field_qa_born_year_circa', 'author_born_circa')->defaultValue(0);
    $this->addFieldMapping('field_qa_death_year', 'author_death');
    $this->addFieldMapping('field_qa_death_year_circa', 'author_death_circa')->defaultValue(0);
    $this->addFieldMapping('field_qa_profession', 'author_profession_et');
    $this->addFieldMapping('field_qa_is_tsitaatcom_user', 'is_tsitaatcom_user')->defaultValue(0);
    $this->addFieldMapping('field_qa_wiki_url', 'author_wiki_url_et');
    $this->addFieldMapping('field_qa_url_alias', 'url_friendly_author_name_et');
    $this->addFieldMapping('field_qa_id', 'id');
    $this->addFieldMapping('field_active', 'active')->defaultValue(0);
  }

  public function prepare($node, $row) {
    _tsitaatcom_row_cleanup($node);
  }

  /**
   * Implements prepareRow().
   */
  public function prepareRow($current_row) {
    /*
    $current_row->temp_id = $current_row->id;
    $current_row->temp_field_author_born_death_circa = $current_row->author_born_death_circa;
    $current_row->temp_field_author_born_circa = $current_row->author_born_circa;
    $current_row->temp_field_author_death_circa = $current_row->author_death_circa;
    $current_row->temp_field_is_tsitaatcom_user = $current_row->is_tsitaatcom_user;
    $current_row->temp_field_active = $current_row->active;*/
  }

  /**
   * Implements complete().
   *
   * Change status and id.
   */
  public function complete($node, $row) {
    $node->field_id[LANGUAGE_NONE][0]['value'] = $row->id;
/*
    if ($row->temp_field_author_born_death_circa == 1) {
      $node->field_author_born_death_circa[LANGUAGE_NONE][0]['value'] = 1;
    }
    else {
      $node->field_author_born_death_circa[LANGUAGE_NONE][0]['value'] = 0;
    }

    if ($row->temp_field_author_born_circa == 1) {
      $node->field_author_born_circa[LANGUAGE_NONE][0]['value'] = 1;
    }
    else {
      $node->field_author_born_circa[LANGUAGE_NONE][0]['value'] = 0;
    }

    if ($row->temp_field_author_death_circa == 1) {
      $node->field_author_death_circa[LANGUAGE_NONE][0]['value'] = 1;
    }
    else {
      $node->field_author_death_circa[LANGUAGE_NONE][0]['value'] = 0;
    }

    if ($row->temp_field_is_tsitaatcom_user == 1) {
      $node->field_is_tsitaatcom_user[LANGUAGE_NONE][0]['value'] = 1;
    }
    else {
      $node->field_is_tsitaatcom_user[LANGUAGE_NONE][0]['value'] = 0;
    }*/

    // Set author relations for quotations.
    _tsitaatcom_migrate_set_author_relations($node);

    // Add author portrait.
    _tsitaatcom_migrate_attach_author_portrait($node);

    field_attach_update('node', $node);
  }
}

/*
 * Migrate tsitaat.com quote sources.
 */
class TsitaatComSourcesMigration extends Migration {

  /**
   * Implements __construct().
   */
  public function __construct() {

    $this->dependencies = array('TsitaatComQuotes');

    parent::__construct();

    $connection = Database::getConnection('default', 'tsitaatcom');

    $query = $connection->select('a_tsitaatcom2_sources', 'a_tsitaatcom2_sources');

    // Get all sources.
    $query
      ->fields(
        'a_tsitaatcom2_sources',
        array(
          'id',
          'name_et',
          'name_en',
          'name_fi',
          'name_de',
          'name_fr',
          'name_hu',
          'name_cs',
          'name_it',
          'name_es',
          'name_dk',
          'url_friendly_name_et',
          'url_friendly_name_en',
          'url_friendly_name_fi',
          'url_friendly_name_de',
          'url_friendly_name_fr',
          'url_friendly_name_hu',
          'url_friendly_name_cs',
          'url_friendly_name_it',
          'url_friendly_name_es',
          'url_friendly_name_dk',
          'source_year_start',
          'source_year_start_circa',
          'source_month_start',
          'source_day_start',
          'source_year_end',
          'source_year_end_circa',
          'url',
          'amazon_product_link_et',
          'isbn_et',
          'amazon_product_link_en',
          'isbn_en',
          'amazon_product_link_de',
          'isbn_de',
        )
      )
      ->orderBy('a_tsitaatcom2_sources.id', 'DESC');


    $this->source = new MigrateSourceSQL($query, array(), NULL, array('map_joinable' => FALSE));

    $this->destination = new MigrateDestinationNode('quote_source');

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

    $this->addFieldMapping('title', 'name_et');
    $this->addFieldMapping('is_new')->defaultValue(TRUE);
    $this->addFieldMapping('uid')->defaultValue(1);
    $this->addFieldMapping('language')->defaultValue('et');

    $this->addFieldMapping('field_qs_id', 'id');
    $this->addFieldMapping('field_qs_name', 'name_et');
    $this->addFieldMapping('field_qs_source_year_start', 'source_year_start');
    $this->addFieldMapping('field_qs_source_year_start_circa', 'source_year_start_circa');
    $this->addFieldMapping('field_qs_source_month_start', 'source_month_start');
    $this->addFieldMapping('field_qs_source_day_start', 'source_day_start');
    $this->addFieldMapping('field_qs_source_year_end', 'source_year_end');
    $this->addFieldMapping('field_qs_source_year_end_circa', 'source_year_end_circa');
    $this->addFieldMapping('field_qs_url', 'url');
    $this->addFieldMapping('field_qs_amazon_product_link', 'amazon_product_link_et');
    $this->addFieldMapping('field_qs_isbn', 'isbn_et');
    $this->addFieldMapping('field_qs_url_alias', 'url_friendly_name_et')->defaultValue('');
  }

  public function prepare($node, $row) {
    _tsitaatcom_row_cleanup($node);
  }

  /**
   * Implements complete().
   */
  public function complete($node, $row) {
    _tsitaatcom_watchdog_quote_source($node);

    // Set source relations for quotations.
    _tsitaatcom_migrate_set_quote_source_relations($node);
  }
}
