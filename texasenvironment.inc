<?php

/**
 * @file Custom code for TCE.
 *
 * Test with:
 * drush @texasenvironment.piglet php-eval '_civicrm_init(); \
 * merge_load_custom_functions(); merge_delete_all(); merge_test();'
 */

// The custom field name and table for the date_started field.
define('TE_START_DATE_ID', 3);
define('TE_START_DATE_FIELD_NAME', 'date_started');
define('TE_START_DATE_TABLE_NAME', 'civicrm_value_constituent_info');

define('TE_STATE_HOUSE_ID', 63);
define('TE_STATE_HOUSE_FIELD_NAME', 'state_assembly_district');
define('TE_STATE_HOUSE_TABLE_NAME', 'civicrm_value_voter_info');

define('TE_DUBIOUS_LOCATION_TYPE_ID', 7);

/**
 * Run the merge hook for batch.
 *
 * This function contains all the logic for manipulating the merge
 * operations.
 */
function merge_custom_batch(&$data, $mainId = NULL, $otherId = NULL, $tables = NULL) {
  $main = $data['old_migration_info']['main_details'];
  $other = $data['old_migration_info']['other_details'];

  merge_debug("Main id: " . $main['contact_id'] . ", Other id: " . $other['contact_id']);
  // Try to set duplicate addresses to dubious before we check for fields
  // in conflict because even if no fields are in conflict we want to deal
  // with these addresses.
  $winning_address = merge_purge_duplicate_addresses($main['contact_id'], $other['contact_id']);

  // Check to see if any fields are in conflict.
  if(count($data['fields_in_conflict']) == 0) {
    // No conflicts, nothing to resolve.
    merge_debug("No fields in conflict.");
    return;
  }

  // Check special rules. Here we pick winners for particular fields.
  // Check for different names.
  if(array_key_exists('move_last_name', $data['fields_in_conflict'])) {
    // Check if they have email address in common.
    if(merge_email_address_in_common($main['contact_id'], $other['contact_id'])) {
      // See if it's just a case-sensitivity issue. If so, we ignore and hope
      // that rules that come next will declare a winner.
      if(strtolower($main['last_name']) != strtolower($other['last_name'])) {
        merge_debug("Common email, different last name, bailing.");
        return;
      }
    }
  }
  else {
    // This means the last name is *not* in conflict...
    if(array_key_exists('move_first_name', $data['fields_in_conflict'])) {
      // And the first name *is* in conflict.
      if(strtolower(substr($main['first_name'], 0, 3)) != strtolower(substr($other['first_name'], 0, 3))) {
        merge_debug("First three letters of first name are different.");
        // And, the first three letters of the name are different (this
        // is trying to catch Kenneith and Ken - which would be the same
        // person as opposed to Kenneth and Bobby which could be a couple.
        if(merge_email_address_in_common($main['contact_id'], $other['contact_id'])) {
          merge_debug("Common email, different first name.");
          // And they have an email address in common - we think they are a
          // couple and we want to merge their names.
          if(preg_match('/&/', $main['first_name']) && preg_match('/&/', $other['first_name'])) {
            // The both have ampersands! Do nothing, let the other rules decide.
            // noop
            merge_debug("Both first names have ampersands. Noop.");
          }
          elseif(preg_match('/&/', $main['first_name'])) {
            $data['fields_in_conflict']['move_first_name'] = $main['first_name'];
            merge_debug("Choosing main first name since it has an ampersand.");
          }
          elseif(preg_match('/&/', $other['first_name'])) {
            $data['fields_in_conflict']['move_first_name'] = $other['first_name'];
            merge_debug("Choosing other first name since it has an ampersand.");
          }
          else {
            // Neither name has ampersand, so merge them with our own ampersand.
            $data['fields_in_conflict']['move_first_name'] = $other['first_name'] .
              ' & ' . $main['first_name'];
            merge_debug("Combining both first names with ampersand.");
          }
        }
      }
      else {
        merge_debug("First three letters of first name are the same.");
        // Same last name, different first name, first three letters of the
        // first name are the same. This might be the same person (e.g.
        // Kenneth and Ken). Check for common email address, or phone
        // number or postal address and if we find it - take the longer first
        // name.
        $email_in_common = merge_email_address_in_common($main['contact_id'], $other['contact_id']);
        $phone_in_common = merge_phone_in_common($main['contact_id'], $other['contact_id']);
        if($email_in_common || $phone_in_common || $winning_address) {
          // We think they are the same person. Which first name is longer?
          merge_debug("Phone or Email or Postal address in common.");
          if(strlen($main['first_name']) > strlen($other['first_name'])) {
            merge_debug("Choosing main first name because it is longer");
            $data['fields_in_conflict']['move_first_name'] = $main['first_name'];
          }
          else {
            merge_debug("Choosing other first name because it is longer or the same length");
            $data['fields_in_conflict']['move_first_name'] = $other['first_name'];
          }
        }
      }
    }
  }

  $main_ext_id = empty($main['external_identifier']) ? 0 : intval($main['external_identifier']);
  $other_ext_id = empty($other['external_identifier']) ? 0 : intval($other['external_identifier']);

  $main_start = merge_get_start_date($main['contact_id']);
  $other_start = merge_get_start_date($other['contact_id']);

  $main_cnt = merge_count_contributions($main['contact_id']);
  $other_cnt = merge_count_contributions($other['contact_id']);

  if($winning_address) {
    merge_debug("Winning address is $winning_address.");
    // If we have voter info fields in conflict, set them to the winning address.
    $voter_field_ids = array(47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,
      63,64,65,66,67,68,69,70,71,83
    );
    while(list(,$field_id) = each($voter_field_ids)) {
      if(array_key_exists('move_custom_' . $field_id, $data['fields_in_conflict'])) {
        merge_debug("Voter field in conflict: $field_id.");
        if($winning_address == $main['contact_id']) {
          merge_debug("Voter field winner is main.");
          $data['fields_in_conflict']['move_custom_' . $field_id] =
            $data['old_migration_info']['rows']['move_custom_' . $field_id]['main'];
        }
        else {
          merge_debug("Voter field winner is other.");
          $data['fields_in_conflict']['move_custom_' . $field_id] =
            $data['old_migration_info']['rows']['move_custom_' . $field_id]['other'];
        }
      }
    }
  }

  // Set winning start date if in conflict.
  if(array_key_exists('move_custom_' . TE_START_DATE_ID, $data['fields_in_conflict'])) {
    merge_debug("start date in conflict");
    if($other_start < $main_start) {
      merge_debug('Using other based on start date chronology');
      $data['fields_in_conflict']['move_custom_' . TE_START_DATE_ID] = $other_start;
    }
    elseif($main_start < $other_start) {
      merge_debug('Using main based on start date chronology');
      $data['fields_in_conflict']['move_custom_' . TE_START_DATE_ID] = $main_start;
    }
  }

  // Set winning external identifier if in conflict
  // Keep track of whether or not we resolve this rule - it may be used below to 
  // determine the dominent record.
  $winning_external_id_record = NULL;
  if(array_key_exists('move_external_identifier', $data['fields_in_conflict'])) {
    merge_debug("external identifier in conflict");
    if($main_ext_id > $other_ext_id) {
      if($main_ext_id < 7000000 || $main_ext_id > 9000000) {
        merge_debug('Using main based on numeric value');
        $data['fields_in_conflict']['move_external_identifier'] = $main_ext_id;
        $winning_external_id_record = $main;
      }
      else {
        merge_debug('Using other based on restricted numeric value');
        $data['fields_in_conflict']['move_external_identifier'] = $other_ext_id;
        $winning_external_id_record = $other;
      }
    }
    elseif($other_ext_id > $main_ext_id) {
      if($other_ext_id < 7000000 || $other_ext_id > 9000000) {
        merge_debug('Using other based on numeric value');
        $data['fields_in_conflict']['move_external_identifier'] = $other_ext_id;
        $winning_external_id_record = $other;
      }
      else {
        merge_debug('Using main based on restricted numeric value');
        $data['fields_in_conflict']['move_external_identifier'] = $main_ext_id;
        $winning_external_id_record = $main;
      }
    }
  }

  // Set winning privacy fields
  $privacy_fields = array( 'do_not_email', 'do_not_phone', 'do_not_mail',
    'do_not_sms', 'do_not_trade', 'is_opt_out');

  while(list(,$field) = each($privacy_fields)) {
    if(array_key_exists("move_$field", $data['fields_in_conflict'])) {
      // Conflicts are always resolved by keeping the privacy setting
      $main_value = empty($main[$field]) ? 0 : intval($main[$field]);
      $other_value = empty($other[$field]) ? 0 : intval($other[$field]);
      if(!empty($main_value) || !empty($other_value)) {
        merge_debug("Setting privacy field ($field) to 1");
        $data['fields_in_conflict']["move_$field"] = 1;
      }
      else{
        merge_debug("Setting privacy field ($field) to 0");
        $data['fields_in_conflict']["move_$field"] = 0;
      }
    }
  }

  // Now try to keep all communications preferences.
  if(array_key_exists("move_preferred_communication_method", $data['fields_in_conflict'])) {
    // Conflicts are always resolved by keeping the privacy setting
    $main_value = $main['preferred_communication_method'];
    $other_value = $other['preferred_communication_method'];
    $result = array_unique(array_merge($other_value, $main_value));
    merge_debug("Merging communications preferences.");
    $data['fields_in_conflict']["move_preferred_communication_method"] = $result;
  }

  // Figure out if we can pick a dominant contact that will override all
  // other fields.
  $winner = NULL;

  // Winner is the one with more contributions.
  if($main_cnt != 0 && $main_cnt > $other_cnt) {
    merge_debug('Winner is main based on contributions');
    $winner = $main;
  }
  elseif($other_cnt != 0 && $other_cnt > $main_cnt) {
    merge_debug('Winner is other based on contributions');
    $winner = $other;
  }
  // If neither winds the countributions test, choose winner of
  // external id test.
  elseif(!is_null($winning_external_id_record)) {
    merge_debug("Picking winner based on external id");
    $winner = $winning_external_id_record;
  }

  if(!is_null($winner)) {
    reset($data['fields_in_conflict']);
    while(list($k, $v) = each($data['fields_in_conflict'])) {
      // Sometimes we have already determined a winner in a previous rule.
      if(!empty($v)) continue;

      $field = preg_replace('/^move_/', '', $k);
      $winning_value = NULL;
      if(empty($winner[$field])) {
        // Try to look it up using other methods
        $winning_value = merge_get_value_for_field($winner['contact_id'], $field);
        if(empty($winning_value)) {
          merge_debug("Winner field is empty. THIS IS A BUG!: $field");
          continue;
        }
      }
      else {
        $winning_value = $winner[$field];
      }
      merge_debug("Setting conflicted field '$k' to " . $winning_value);
      $data['fields_in_conflict'][$k] = $winning_value;
    }
  }
  else {
    merge_debug("No winner determined.");
  }
  // Report to user which fields are still in conflict.
  reset($data['fields_in_conflict']);
  $still_in_conflict = array();
  while(list($k, $v) = each($data['fields_in_conflict'])) {
    if(empty($v)) {
      $still_in_conflict[] = $k;
    }
  }
  if(count($still_in_conflict) > 0) {
    $conflicts = implode(",", $still_in_conflict);
    $session = CRM_Core_Session::singleton();
    $msg = ts("Contact ID: %1: The following fields are in conflict: %2", array(1 => $main['contact_id'], 2 => $conflicts, 'domain' => 'net.ourpowerbase.merge'));
    CRM_Core_Error::debug_log_message($msg);
    $session->setStatus($msg);
  }
}

/**
 * Try to lookup the given field for the given contact.
 *
 */
function merge_get_value_for_field($contact_id, $field) {
  $params = array('id' => $contact_id, 'return' => $field);
  try {
    $result = civicrm_api3('Contact', 'get', $params);
  }
  catch (CiviCRM_API3_Exception $e) {
    merge_debug("Failed to lookup field $field for $contact_id");
    return FALSE;
  }
  $value = array_pop($result['values']);
  return $value[$field];
}

/**
 * Remove oldest address.
 *
 * Choose address associated with last contribution or,
 * if no contributions, with latest start date.
 *
 * @return contact id of the winning address.
 */
function merge_purge_duplicate_addresses($id1, $id2) {
  // Only operate if both ids have an address.
  $ids = array($id1, $id1);
  $sql = "SELECT id, street_address, city, postal_code FROM
    civicrm_address WHERE contact_id = %0";
  $addresses = array();
  while(list(,$id) = each($ids)) {
    $params = array(0 => array($id, 'Integer'));
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    // No winner
    if(empty($dao->id)) return NULL;
  }

  // Set global variable - we don't change any records here - we wait
  // for the civicrm_post hook - after the merge is complete. Then
  // we check this global variable for additional processing.
  global $merge_address_ids;
  if(!is_array($merge_address_ids)) {
    $merge_address_ids = array();
  }

  // Which address is associated with the most recent contribution?
  $sql = "SELECT contact_id FROM civicrm_contribution WHERE contact_id
    IN (%0, %1) ORDER BY receive_date DESC LIMIT 1";
  $params = array(
    0 => array($id1, 'Integer'),
    1 => array($id2, 'Integer'),
  );
  $dao = CRM_Core_DAO::executeQuery($sql, $params);
  $dao->fetch();

  if(!empty($dao->contact_id)) {
    if($dao->contact_id == $id1) {
      merge_debug("Dubiousifying id2 address based on contributions.");
      $merge_address_ids[] = array(
        'contact_ids' => $ids,
        'dubious_address_ids' => merge_get_address_ids($id2),
      );
      return $id1;
    }
    elseif($dao->contact_id == $id2) {
      merge_debug("Dubiousifying id1 address based on contributions.");
      $merge_address_ids[] = array(
        'contact_ids' => $ids,
        'dubious_address_ids' => merge_get_address_ids($id1),
      );
      return $id2;
    }
  }
  // Now check start date
  $id1_start = merge_get_start_date($id1);
  $id2_start = merge_get_start_date($id2);

  if($id1_start > $id2_start) {
    merge_debug("Dubiousifying id2 address based on start date.");
    $merge_address_ids[] = array(
        'contact_ids' => $ids,
        'dubious_address_ids' => merge_get_address_ids($id2),
    );
    return $id1;
  }
  elseif($id2_start > $id1_start) {
    merge_debug("Dubiousifying id1 address based on start date.");
    $merge_address_ids[] = array(
        'contact_ids' => $ids,
        'dubious_address_ids' => merge_get_address_ids($id1),
    );
    return $id2;
  }
  return NULL;
}

/**
 * Set addresses to dubious.
 *
 * Or delete it if it's the same as a non-dubious one.
 */
function merge_set_addresses_to_dubious($contact_id, $address_ids) {
  $sql = "SELECT id, street_address, city, postal_code FROM civicrm_address
    WHERE contact_id = %0";
  $dao = CRM_Core_DAO::executeQuery($sql, array(0 => array($contact_id, 'Integer')));
  $good = array();
  $dubious = array();
  while($dao->fetch()) {
    if(!in_array($dao->id, $address_ids)) {
      // we assume only one good address
      $good = $dao->street_address . $dao->city . $dao->postal_code;
    }
    else {
      $dubious[$dao->id] = $dao->street_address . $dao->city . $dao->postal_code;
    }
  }
  while(list($id, $value) = each($dubious)) {
    if($value == $good) {
      // Delete it. It's the same.
      merge_debug("Deleting identical address.");
      $sql = "DELETE FROM civicrm_address WHERE id = %0";
      CRM_Core_DAO::executeQuery($sql, array(0 => array($id, 'Integer')));
    }
    else {
      merge_debug("Setting address id $id to dubious.");
      $sql = "UPDATE civicrm_address SET location_type_id = %0,
        is_primary = 0 WHERE id = %1";
      $params = array(
        0 => array(TE_DUBIOUS_LOCATION_TYPE_ID, 'Integer'),
        1 => array($id, 'Integer')
      );
      CRM_Core_DAO::executeQuery($sql, $params);
    }
  }
}

/**
 * Implementation of hook_civicrm_post.
 */
function merge_civicrm_post($op, $objectName, $objectId, &$objectRef) {
  if($objectName == 'Individual' && $op == 'edit') {
    global $merge_address_ids;
    if(count($merge_address_ids) > 0) {
      while(list(,$values) = each($merge_address_ids)) {
        // See if we have a matching contact. We're not sure which contact_id
        // was deleted so we need to check for both.
        if(in_array($objectId, $values['contact_ids'])) {
          merge_set_addresses_to_dubious($objectId, $values['dubious_address_ids']);
          // Now ensure that there is a remaining address that is
          // primary and set to home.
          $sql = "SELECT id FROM civicrm_address WHERE location_type_id
            = 1 AND contact_id = %1 AND is_primary = 1";
          $params = array(
            1 => array($objectId, 'Integer'),
          );
          $dao = CRM_Core_DAO::executeQuery($sql, $params);
          if($dao->N == 0) {
            merge_debug("Contact ID $objectId doesn't have a primary address.");
            // Set address to is_primary
            $sql = "UPDATE civicrm_address SET is_primary = 1, location_type_id = 1 WHERE
              contact_id = %0 AND location_type_id != %1 LIMIT 1";
            $params = array(
              0 => array($objectId, 'Integer'),
              1 => array(TE_DUBIOUS_LOCATION_TYPE_ID, 'Integer'),
            );
            CRM_Core_DAO::executeQuery($sql, $params);
          }
        }
      }
    }
  }
}

/**
 * Helper for deleting related address.
 */
function merge_get_address_ids($contact_id) {
  $ret = array();
  $sql = "SELECT id FROM civicrm_address WHERE contact_id = %0";
  $params = array(0 => array($contact_id, 'Integer'));
  $dao = CRM_Core_DAO::executeQuery($sql, $params);
  while($dao->fetch()) {
    $ret[] = $dao->id;
  }
  return $ret;
}

/**
 * Check for common email address
 *
 * Given the two contact ids, return TRUE if they have an email address in
 * common or FALSE if they don't.
 */
function merge_email_address_in_common($id1, $id2) {
  $sql = "SELECT COUNT(e1.email) AS count FROM civicrm_email e1 JOIN civicrm_email e2
    ON e1.email = e2.email WHERE e1.contact_id IN (%0,%1) AND e2.contact_id
    IN (%0,%1) GROUP BY e1.contact_id HAVING COUNT(e1.contact_id) > 1";
  $params = array(
    0 => array($id1, 'Integer'),
    1 => array($id2, 'Integer')
  );
  $dao = CRM_Core_DAO::executeQuery($sql, $params);
  $dao->fetch();
  // If neither have an email address, this won't return any results.
  if($dao->N == 0) return FALSE;
  if($dao->count == 0) return FALSE;
  return TRUE;
}

/**
 * Check for common phone number 
 *
 * Given the two contact ids, return TRUE if they have a phone number in
 * common or FALSE if they don't.
 */
function merge_phone_in_common($id1, $id2) {
  $sql = "SELECT COUNT(p1.phone_numeric) AS count FROM civicrm_phone p1 JOIN civicrm_phone p2
    ON p1.phone_numeric = p2.phone_numeric WHERE p1.contact_id IN (%0,%1) AND p2.contact_id
    IN (%0,%1) GROUP BY p1.contact_id HAVING COUNT(p1.contact_id) > 1";
  $params = array(
    0 => array($id1, 'Integer'),
    1 => array($id2, 'Integer')
  );
  $dao = CRM_Core_DAO::executeQuery($sql, $params);
  $dao->fetch();
  // If neither have an email address, this won't return any results.
  if($dao->N == 0) return FALSE;
  if($dao->count == 0) return FALSE;
  return TRUE;
}
/**
 * Return the start date for the given contact.
 *
 */
function merge_get_start_date($contact_id) {
  $sql = "SELECT " . TE_START_DATE_FIELD_NAME . " AS start_date FROM "
      . TE_START_DATE_TABLE_NAME . " WHERE entity_id = %0";
  $dao = CRM_Core_DAO::executeQuery($sql, array(0 => array($contact_id, 'Integer')));
  $dao->fetch();
  return $dao->start_date;
}

/**
 * Count how many contributions a contact_id has.
 *
 *
 */
function merge_count_contributions($contact_id) {
  if(empty($contact_id)) {
    echo "Error!\n\n";
    return 0;
  }
  $sql = "SELECT COUNT(*) AS count FROM civicrm_contribution WHERE is_test = 0
    AND contact_id = %0";
  $dao = CRM_Core_DAO::executeQuery($sql, array(0 => array($contact_id, 'Integer')));
  $dao->fetch();
  return $dao->count;
}

/**
 * Return an array of function names that should be called when testing.
 *
 */
function merge_get_tests() {
  // Our tests require permanent deletion so we have to be sure we have the
  // correct permissions.
  global $user;
  $user->uid = 1;

  return array(
    'merge_test_contributions',
    'merge_test_start_date',
    'merge_test_ext_id_numerical_value',
    'merge_test_ext_id_numerical_value_restricted',
    'merge_test_email_last_name_bail',
    'merge_test_email_first_name_merge',
    'merge_test_email_first_name_choose',
    'merge_test_address',
    'merge_test_privacy',
    'merge_test_comm_preferences',
    'merge_test_voter_info',
    'merge_test_first_three'
  );
}

/**
 * Merge records with same last name and different first
 *
 * If first three letters are the same.
 */
function merge_test_first_three() {
  merge_debug("Running " . __function__);
  $rand = substr(md5(rand()), 0, 5);
  // Create two records with same phone but different first name). Ensure
  // first name has same first three characters.
  $c1_id = merge_create_contact(
    $rand,
    array('first_name' => 'Kenneth' . $rand, 'last_name' => 'Smith' . $rand)
  );
  $c2_id = merge_create_contact($rand,
    array('first_name' => 'Ken' . $rand, 'last_name' => 'Smith' . $rand)
  );
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);
  // Add same phone number
  $params = array(
    'contact_id' => $c1_id,
    'phone' => '(402)555-0870',
    'location_type_id' => 1,
  );
  if(!merge_create('Phone', $params)) {
    return FALSE;
  }
  $params = array(
    'contact_id' => $c2_id,
    'phone' => '402-555-0870',
    'location_type_id' => 1,
  );
  if(!merge_create('Phone', $params)) {
    return FALSE;
  }

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Same email, same last name, different first name, same first three characters';
  if(count($ret['merged']) == 1) {
    merge_report('success', "$test: records merged.");
    // Check to see if they were properly merged.
    $sql = "SELECT first_name FROM civicrm_contact WHERE id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    if($dao->first_name == 'Kenneth' . $rand) {
      merge_report('success', "$test: Kenneth properly chosen as winning first name.");
    }
    else {
      merge_report('fail', "$test: first name should have been Kenneth, instead it is: " . $dao->first_name);
    }
  }
  elseif(count($ret['skipped']) == 1) {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;


}
/**
 * Ensure all enabled communications preferences are preserved.
 *
 * Ensure all are copied..
 */
function merge_test_comm_preferences() {
  merge_debug("Running " . __function__);
  $rand = substr(md5(rand()), 0, 5);
  // Create two records with different start dates.
  $params = array(
    'preferred_communication_method' => array( 1,2 )
  );
  $c1_id = merge_create_contact($rand, $params);
  $params = array(
    'preferred_communication_method' => array( 2,3 )
  );

  $c2_id = merge_create_contact($rand, $params);
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Privacy Settings';
  if(count($ret['merged']) == 1) {
    merge_report('success', "$test: records merged.");
    // Check to see if they were properly merged.
    $sql = "SELECT preferred_communication_method FROM civicrm_contact WHERE id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    $comm_prefs = explode(CRM_Core_DAO::VALUE_SEPARATOR, $dao->preferred_communication_method);

    $nums = array(1,2,3);
    while(list(,$k) = each($nums)) {
      if(in_array($k, $comm_prefs)) {
        merge_report('success', "$test: Communications preference $k is included.");
      }
      else {
        merge_report('success', "$test: Communications preference $k is not included.");
      }
    }
  }
  elseif(count($ret['skipped']) == 1) {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}

/**
 * Ensure all enabled privacy flags are preserved.
 *
 * Favor all privacy flags that are enabled.
 */
function merge_test_privacy() {
  merge_debug("Running " . __function__);
  $rand = substr(md5(rand()), 0, 5);
  // Create two records with different start dates.
  $params = array(
   'do_not_email' => 1,
   'do_not_phone' => 0,
   'do_not_mail' => 1,
   'do_not_sms' => 0,
   'do_not_trade' => 0,
   'is_opt_out' => 0
  );

  $c1_id = merge_create_contact($rand, $params);
  $params = array(
   'do_not_email' => 0,
   'do_not_phone' => 1,
   'do_not_mail' => 0,
   'do_not_sms' => 1,
   'do_not_trade' => NULL,
   'is_opt_out' => 1
  );

  $c2_id = merge_create_contact($rand, $params);
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Privacy Settings';
  if(count($ret['merged']) == 1) {
    merge_report('success', "$test: records merged.");
    // Check to see if they were properly merged.
    $sql = "SELECT do_not_email, do_not_phone, do_not_mail, do_not_sms,
      do_not_trade, is_opt_out FROM civicrm_contact WHERE id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    // All privacy params should be set to 1 except do_not_trade
    if($dao->do_not_trade == 1) {
      merge_report('fail', "$test: privacy param do_not_trade set to 1");
    }
    else {
      merge_report('success', "$test: privacy param do_not_trade set to 0");
    }
    if($dao->do_not_email == "1") {
      merge_report('success', "$test: privacy param do_not_email set to 1");
    }
    else {
      merge_report('fail', "$test: privacy param do_not_email set to 0");
    }
    if($dao->do_not_phone == 1) {
      merge_report('success', "$test: privacy param do_not_phone set to 1");
    }
    else {
      merge_report('fail', "$test: privacy param do_not_phone set to 0");
    }
    if($dao->do_not_sms == 1) {
      merge_report('success', "$test: privacy param do_not_sms set to 1");
    }
    else {
      merge_report('fail', "$test: privacy param do_not_sms set to 0");
    }
    if($dao->is_opt_out == 1) {
      merge_report('success', "$test: privacy param is_opt_out set to 1");
    }
    else {
      merge_report('fail', "$test: privacy param is_opt_out set to 0");
    }
  }
  elseif(count($ret['skipped']) == 1) {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}

/**
 * Ensure voter info goes with winning address.
 */
function merge_test_voter_info() {
  merge_debug("Running " . __function__);
  $rand = substr(md5(rand()), 0, 5);
  // Create two records with different start dates.
  // And different state house district.
  $params = array(
    'custom_' . TE_START_DATE_ID => '2010-01-02',
    'custom_' . TE_STATE_HOUSE_ID => 10,
  );
  $c1_id = merge_create_contact($rand, $params);
  $params = array(
    'custom_' . TE_START_DATE_ID => '2010-01-01',
    'custom_' . TE_STATE_HOUSE_ID => 20,
  );
  $c2_id = merge_create_contact($rand, $params);
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Add different addresses
  $params = array(
    'contact_id' => $c1_id,
    'street_address' => '123 4th Avenue',
    'location_type_id' => 1,
    'city' => 'Houston',
  );
  if(!merge_create('Address', $params)) {
    return FALSE;
  }
  // Add different addresses
  $params = array(
    'contact_id' => $c2_id,
    'street_address' => '456 4th Avenue',
    'location_type_id' => 1,
    'city' => 'Houston',
  );
  if(!merge_create('Address', $params)) {
    return FALSE;
  }

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Voter Info';
  if(count($ret['merged']) == 1) {
    merge_report('success', "$test: records merged.");
    // Check to see if they were properly merged.
    $sql = "SELECT " . TE_STATE_HOUSE_FIELD_NAME . " AS state_house FROM
      civicrm_contact c JOIN " . TE_STATE_HOUSE_TABLE_NAME . " sh ON 
      c.id = sh.entity_id WHERE c.id IN (%0, %1) AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    if($dao->state_house == '10') {
      merge_report('success', "$test: proper state house district was chosen.");
    }
    else {
      merge_report('fail', "$test: wrong state house district was chosen.");
    }
  }
  elseif(count($ret['skipped']) == 1) {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}
/**
 * Winning address is one with latest contribution or start date.
 *
 * Set losing addresses to dubious location type id.
 * The contact with the most recent contribution should
 * win - or, if no contributions, the one with the most
 * recent start date should win.
 */
function merge_test_address() {
  merge_debug("Running " . __function__);
  $rand = substr(md5(rand()), 0, 5);
  // Create two records with different start dates.
  // and contributions.
  // contact 1 has older start date, but more recent contribution
  // and should therefore win.
  $params = array(
    'custom_' . TE_START_DATE_ID => '2010-01-01'
  );
  $c1_id = merge_create_contact($rand, $params);
  $params = array(
    'custom_' . TE_START_DATE_ID => '2010-01-02'
  );
  $c2_id = merge_create_contact($rand, $params);
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Add different addresses
  $params = array(
    'contact_id' => $c1_id,
    'street_address' => '123 4th Avenue',
    'location_type_id' => 1,
    'city' => 'Houston',
  );
  if(!merge_create('Address', $params)) {
    return FALSE;
  }

  // Add contribution
  $params = array(
    'contact_id' => $c1_id,
    'financial_type_id' => 1,
    'total_amount' => '5.00',
    'receive_date' => '2014-12-02'
  );

  if(!merge_create('Contribution', $params)) {
    return FALSE;
  }

  // Add different addresses
  $params = array(
    'contact_id' => $c2_id,
    'street_address' => '456 4th Avenue',
    'location_type_id' => 1,
    'city' => 'Houston',
  );
  if(!merge_create('Address', $params)) {
    return FALSE;
  }

  // Add contribution
  $params = array(
    'contact_id' => $c2_id,
    'financial_type_id' => 1,
    'total_amount' => '5.00',
    'receive_date' => '2014-12-01'
  );
  if(!merge_create('Contribution', $params)) {
    return FALSE;
  }
  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Postal address';
  if(count($ret['merged']) == 1) {
    merge_report('success', "$test: records merged.");
    // Check to see if they were properly merged.
    $sql = "SELECT location_type_id, street_address, is_primary FROM civicrm_contact c JOIN
      civicrm_address a ON c.id = a.contact_id WHERE c.id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $main_test = FALSE;
    $dubious_test = FALSE;
    while($dao->fetch()) {
      if($dao->street_address == '123 4th Avenue' && $dao->is_primary == 1) {
        $main_test = TRUE;
        merge_report('success', "$test: proper address was chosen as main address.");
      }
      if($dao->street_address == '456 4th Avenue' && $dao->location_type_id == TE_DUBIOUS_LOCATION_TYPE_ID) {
        $dubious_test = TRUE;
        merge_report('success', "$test: proper address was chosen as dubious address.");
      }
    }
    if(!$main_test) {
      merge_report('fail', "$test: proper address was not chosen as main address.");
    }
    if(!$dubious_test) {
      merge_report('fail', "$test: proper address was not chosen as dubious address.");
    }
  }
  elseif(count($ret['skipped']) == 1) {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}

/**
 * Test merging on matches with same email, same last name, different
 * first_names but one already has ampersand.
 *
 */
function merge_test_email_first_name_choose() {
  merge_debug("Running " . __function__);
  $rand = substr(md5(rand()), 0, 5);
  // Create two records with same email but different first name). Also, 
  // add external ids so they have a reason to otherwise merge.
  $c1_id = merge_create_contact($rand, array('first_name' => 'Lucy & Simone' . $rand, 'external_identifier' => 100));
  $c2_id = merge_create_contact($rand, array('first_name' => 'Simone' . $rand, 'external_identifier' => 101));
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Same email, same last name, different first name';
  if(count($ret['merged']) == 1) {
    merge_report('success', "$test: records merged.");
    // Check to see if they were properly merged.
    $sql = "SELECT first_name FROM civicrm_contact WHERE id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    if($dao->first_name == 'Lucy & Simone' . $rand) {
      merge_report('success', "$test: first name with ampersand properly chosen.");
    }
    else {
      merge_report('fail', "$test: first name should be ampersanded choice, instead it is: " . $dao->first_name);
    }
  }
  elseif(count($ret['skipped']) == 1) {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}
/**
 * Test merging on matches with same email, same last name, different
 * first_names.
 *
 */
function merge_test_email_first_name_merge() {
  merge_debug("Running " . __function__);
  $rand = substr(md5(rand()), 0, 5);
  // Create two records with same email but different first name). Also, 
  // add external ids so they have a reason to otherwise merge.
  $c1_id = merge_create_contact($rand, array('first_name' => 'Lucy' . $rand, 'external_identifier' => 100));
  $c2_id = merge_create_contact($rand, array('first_name' => 'Simone' . $rand, 'external_identifier' => 101));
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Same email, same last name, different first name';
  if(count($ret['merged']) == 1) {
    merge_report('success', "$test: records merged.");
    // Check to see if they were properly merged.
    $sql = "SELECT first_name FROM civicrm_contact WHERE id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    if($dao->first_name == 'Simone' . $rand . ' & Lucy' . $rand) {
      merge_report('success', "$test: first name merged with ampersand.");
    }
    else {
      merge_report('fail', "$test: first name should have ampersand, instead it is: " . $dao->first_name);
    }
  }
  elseif(count($ret['skipped']) == 1) {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}
/**
 * Test bailing on matches with  same email, different last names.
 *
 */
function merge_test_email_last_name_bail() {
  merge_debug("Running " . __function__);
  $rand = md5(rand());
  // Create two records with same email but different last name). Also, 
  // add external ids so they have a reason to otherwise merge.
  $c1_id = merge_create_contact($rand, array('last_name' => 'Smith' . $rand, 'external_identifier' => 100));
  $c2_id = merge_create_contact($rand, array('last_name' => 'Perez' . $rand, 'external_identifier' => 101));
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Same email, different last name';
  if(count($ret['merged']) == 1) {
    merge_report('fail', "$test: records merged, when they should not be merged.");
  }
  elseif(count($ret['skipped']) == 1) {
    merge_report('success', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}

/**
 * Test selection of ext id based on numerical value when in restricted range.
 *
 * If two records have external identifier and the one with the
 * higher external id is between 7 million and 9 million, choose the one with
 * a lower numerical value. Also, set as dominent record and overwrite
 * middle name.
 */
function merge_test_ext_id_numerical_value_restricted() {
  merge_debug("Running " . __function__);
  $rand = md5(rand());
  $c1_id = merge_create_contact($rand, array('external_identifier' => 7100003, 'middle_name' => 'Sally'));
  $c2_id = merge_create_contact($rand, array('external_identifier' => 101, 'middle_name' => 'Lou'));
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'External ID: numerical value restricted';
  if(count($ret['merged']) == 1) {
    // So far so good.
    merge_report('success', "$test: records merged");
    // Now let's see if the correct external id and middle name was selected .
    $sql = "SELECT external_identifier, middle_name FROM civicrm_contact WHERE id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    if($dao->N != 1) {
      merge_report('fail', "$test: More than one result after merging." . $dao->N . "$c1_id and $c2_id");
    }
    else{
      if($dao->external_identifier == 101) {
        merge_report('success', "$test: retained proper id");
      }
      else {
        merge_report('fail', "$test: retained wrong id: " . $dao->external_identifier);
      }
      if($dao->middle_name == 'Lou') {
        merge_report('success', "$test: retained proper middle name");
      }
      else {
        merge_report('fail', "$test: retained wrong middle name");
      }
    }
  }
  else {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}

/**
 * Test selection of external identifier based on numerical value.
 *
 * If two records have external identifier choose the one with
 * a higher numerical value.
 */
function merge_test_ext_id_numerical_value() {
  merge_debug("Running " . __function__);

  $rand = md5(rand());
  $c1_id = merge_create_contact($rand, array('external_identifier' => 100));
  $c2_id = merge_create_contact($rand, array('external_identifier' => 101));
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'External ID: numerical value, not restricted';
  if(count($ret['merged']) == 1) {
    // So far so good.
    merge_report('success', "$test: records merged");
    // Now let's see if the correct external id was selected .
    $sql = "SELECT external_identifier FROM civicrm_contact WHERE id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    if($dao->N != 1) {
      merge_report('fail', "$test: More than one result after merging." . $dao->N . "$c1_id and $c2_id");
    }
    elseif($dao->external_identifier == 101) {
      merge_report('success', "$test: retained proper id");
    }
    else {
      merge_report('fail', "$test: retained wrong id: " . $dao->external_identifier);
    }
  }
  else {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}

/**
 * Test selection of external identifier based on earliest start date.
 *
 * If two records have a different start date, pick the earliest.
 */
function merge_test_start_date() {
  merge_debug("Running " . __function__);
  $rand = md5(rand());

  $params = array(
    'custom_' . TE_START_DATE_ID => '2010-01-02'
  );
  $c1_id = merge_create_contact($rand, $params);
  $params = array(
    'custom_' . TE_START_DATE_ID => '2010-01-01'
  );
  $c2_id = merge_create_contact($rand, $params);
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Different start date';
  if(count($ret['merged']) == 1) {
    // So far so good.
    merge_report('success', "$test: records merged");
    // Now let's see if the  correct external id was selected .
    $sql = "SELECT " . TE_START_DATE_FIELD_NAME . " AS start_date FROM civicrm_contact c
      JOIN " . TE_START_DATE_TABLE_NAME . " sd ON c.id = sd.entity_id WHERE c.id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    if(substr($dao->start_date, 0, 10) == '2010-01-01') {
      merge_report('success', "$test: retained proper id");
    }
    else {
      merge_report('fail', "$test: retained wrong start date: " . $dao->start_date);
    }
  }
  else {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}
/**
 * Test selection of external identifier based on contributions.
 *
 * If two records have any field in conflict and one has more contributions
 * then the other, favor all fields with more contributions. 
 */
function merge_test_contributions() {
  merge_debug("Running " . __function__);
  $c1_id = merge_create_contact($rand, array('middle_name' => 'Sally'));
  $c2_id = merge_create_contact($rand, array('middle_name' => 'Lou'));
  if(!$c1_id || !$c2_id) return FALSE;

  $contact_ids = array($c1_id, $c2_id);

  // Add two contributions
  $params = array(
    'contact_id' => $c1_id,
    'financial_type_id' => 1, // donation
    'total_amount' => '100.00',
  );
  if(!merge_create('Contribution', $params)) {
    return FALSE;
  }
  $params = array(
    'contact_id' => $c1_id,
    'financial_type_id' => 1, // donation
    'total_amount' => '100.00',
  );
  if(!merge_create('Contribution', $params)) {
    return FALSE;
  }

  // Add one contributions
  $params = array(
    'contact_id' => $c2_id,
    'financial_type_id' => 1, // donation
    'total_amount' => '100.00',
  );
  if(!merge_create('Contribution', $params)) {
    return FALSE;
  }

  // Test batch merging.
  $pairs = array(
    array(
      'dstID' => $c1_id,
      'srcID' => $c2_id
    )
  );
  $ret = CRM_Dedupe_Merger::merge($pairs);
  $test = 'Different # of contributions';
  if(count($ret['merged']) == 1) {
    // So far so good.
    merge_report('success', "$test: records merged");
    // Now let's see if the  correct external id was selected and make
    // sure all three contributions remain.
    $sql = "SELECT middle_name FROM civicrm_contact WHERE id IN (%0, %1)
      AND is_deleted = 0";
    $params = array(
      0 => array($c1_id, 'Integer'),
      1 => array($c2_id, 'Integer'),
    );
    $dao = CRM_Core_DAO::executeQuery($sql, $params);
    $dao->fetch();
    if($dao->middle_name == 'Sally') {
      merge_report('success', "$test: retained proper middle name");
    }
    else {
      merge_report('fail', "$test: retained wrong middle name: " . $dao->middle_name);
    }
  }
  else {
    merge_report('fail', "$test: records not merged");
  }
  // Now clean up
  merge_clean_up('Contact', $contact_ids);
  return TRUE;
}

/**
 * Output final results of the report.
 */
function merge_test_report_final() {
  merge_report('success', 'Tests complete', TRUE);
  return TRUE;
}

/**
 * Delete all records created by these tests.
 *
 */
function merge_delete_all() {
  _civicrm_init();
  $sql = "SELECT id FROM civicrm_contact WHERE external_identifier IN (100, 101, 7100003)";
  $dao = CRM_Core_DAO::executeQuery($sql);
  $ids = array();
  while($dao->fetch()) {
    if($dao->id) {
        $ids[] = $dao->id;
    }
  }
  global $user;
  $user->uid = 1;
  merge_load_custom_functions();
  merge_clean_up('Contact', $ids);
}
