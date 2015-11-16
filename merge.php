<?php

require_once 'merge.civix.php';

/**
 * Implementation of hook_civicrm_config
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_config
 */
function merge_civicrm_config(&$config) {
  _merge_civix_civicrm_config($config);
}

/**
 * Implementation of hook_civicrm_xmlMenu
 *
 * @param $files array(string)
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_xmlMenu
 */
function merge_civicrm_xmlMenu(&$files) {
  _merge_civix_civicrm_xmlMenu($files);
}

/**
 * Implementation of hook_civicrm_install
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_install
 */
function merge_civicrm_install() {
  return _merge_civix_civicrm_install();
}

/**
 * Implementation of hook_civicrm_uninstall
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_uninstall
 */
function merge_civicrm_uninstall() {
  return _merge_civix_civicrm_uninstall();
}

/**
 * Implementation of hook_civicrm_enable
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_enable
 */
function merge_civicrm_enable() {
  return _merge_civix_civicrm_enable();
}

/**
 * Implementation of hook_civicrm_disable
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_disable
 */
function merge_civicrm_disable() {
  return _merge_civix_civicrm_disable();
}

/**
 * Implementation of hook_civicrm_upgrade
 *
 * @param $op string, the type of operation being performed; 'check' or 'enqueue'
 * @param $queue CRM_Queue_Queue, (for 'enqueue') the modifiable list of pending up upgrade tasks
 *
 * @return mixed  based on op. for 'check', returns array(boolean) (TRUE if upgrades are pending)
 *                for 'enqueue', returns void
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_upgrade
 */
function merge_civicrm_upgrade($op, CRM_Queue_Queue $queue = NULL) {
  return _merge_civix_civicrm_upgrade($op, $queue);
}

/**
 * Implementation of hook_civicrm_managed
 *
 * Generate a list of entities to create/deactivate/delete when this module
 * is installed, disabled, uninstalled.
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_managed
 */
function merge_civicrm_managed(&$entities) {
  return _merge_civix_civicrm_managed($entities);
}

/**
 * Implementation of hook_civicrm_caseTypes
 *
 * Generate a list of case-types
 *
 * Note: This hook only runs in CiviCRM 4.4+.
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_caseTypes
 */
function merge_civicrm_caseTypes(&$caseTypes) {
  _merge_civix_civicrm_caseTypes($caseTypes);
}

/**
 * Implementation of hook_civicrm_alterSettingsFolders
 *
 * @link http://wiki.civicrm.org/confluence/display/CRMDOC/hook_civicrm_alterSettingsFolders
 */
function merge_civicrm_alterSettingsFolders(&$metaDataFolders = NULL) {
  _merge_civix_civicrm_alterSettingsFolders($metaDataFolders);
}


/**
 * Load custom functions for this site.
 *
 */
function merge_load_custom_functions() {
  // Include domain specific file
  $url_pieces = parse_url(CIVICRM_UF_BASEURL);
  $host_pieces = explode('.', $url_pieces['host']);
  if(file_exists(__dir__ . '/' . $host_pieces[0] . '.inc')) {
    require_once($host_pieces[0] . '.inc');
  }
}

/**
 * Implementation of hook_civicrm_merge
 **/
function merge_civicrm_merge($type, &$data, $mainId = NULL, $otherId = NULL, $tables = NULL) {
  merge_load_custom_functions();    
  if($type == 'batch') {
    if(!function_exists('merge_custom_batch')) {
      $session = CRM_Core_Session::singleton();
      $msg = ts("Failed to load custom batch function. Maybe a domain name mis-match?");
      $session->setStatus($msg);
      return;
    }
    merge_custom_batch($data, $mainId = NULL, $otherId = NULL, $tables = NULL);
  }
}

/**
 * Call all tests 
 *
 */
function merge_test($run_tests = array()) {
  merge_load_custom_functions();    
  $tests = merge_get_tests();
  reset($tests);
  while(list(,$test) = each($tests)) {
    $res = TRUE;
    if(count($run_tests) > 0) {
      // Only run if the tests is specified
      if(in_array($test, $run_tests)) {
        $res = $test();
      }
    }
    else {
      $res = $test();
    }
    if(!$res) {
      merge_report('fail', "$test had an error");
    }
  }
  merge_test_report_final();
}

/**
 * Helper function to output errors
 */
function merge_debug($err, $severity = 'notice') {
  
  if(php_sapi_name() === 'cli') {
    CRM_Core_Error::debug(NULL, $err, TRUE, FALSE);
  }
  elseif(function_exists('dsm')) {
    dsm($err);
  }
}

/**
 * Helper function to gather reports of tests that have failed or succeeded.
 *
 *
 */
function merge_report($status, $msg, $print = FALSE) {
  static $report = array();
  $report[$status][] = $msg;
  if(!function_exists('drush_print')) {
    return;
  }
  if($print) {
    while(list($key, $value) = each($report)) {
      if(count($value) > 0) {
        drush_print("== " . strtoupper($key) . "==");
        drush_print(implode("\n", $value));
      }
    }
  }
}
/**
 * Helper function for creating entities and checking for errors.
 *
 */
function merge_create($entity, $params) {
  if($entity == 'Contact') {
    if(!array_key_exists('contact_type', $params)) {
      $params['contact_type'] = 'Individual';
    }
  }
  try{
    $result = civicrm_api3($entity, 'create', $params);
  }
  catch(CiviCRM_API3_Exception $e) {
    $err = $e->getMessage();
    merge_debug(ts("Error creating $entity: ") . $err, 'error');
    return FALSE;
  }
  $ret = array_pop($result['values']);
  return $ret;
}

/**
  * Helper function to create contacts with same random values. 
  *
 */
function merge_create_contact($rand, $extra = array()) {
  $email = 'test' . $rand . '@example.org';
  $params = array(
    'first_name' => 'Test First Name' . $rand,
    'last_name' => 'Test Last Name' . $rand,
    'email' => $email,
  );
  while(list($k, $v) = each($extra)) {
    $params[$k] = $v;
  }
  $contact = merge_create('Contact', $params);
  if(!$contact) return FALSE;
  return $contact['id'];
}

/**
 * Helper function for deleting entities creating during this run.
 *
 */
function merge_clean_up($entity, $ids) {
  try{
    reset($ids);
    while(list(,$id) = each($ids)) {
      $params = array(
        'id' => $id,
      );
      if($entity == 'Contact') {
        $params['skip_undelete'] = TRUE;
        // Manually delete any contributions to avoid delete errors
        $sql = "DELETE FROM civicrm_contribution WHERE contact_id = %0";
        CRM_Core_DAO::executeQuery($sql, array(0 => array($id, 'Integer')));
      }
      $result = civicrm_api3($entity, 'delete', $params);
    }
  }
  catch(CiviCRM_API3_Exception $e) {
    $err = $e->getMessage();
    merge_debug(ts("Error deleting $entity: ") .$err, 'error');
    return FALSE;
  }
  return TRUE;
}
