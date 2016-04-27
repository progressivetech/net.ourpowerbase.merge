# Merge

This CiviCRM extension provides a framework for creating custom auto merge rules.

CiviCRM has a core feature that allows you to batch merge groups of contacts in
which there are no conflicts.

However, some organizations need to batch merge records with conflicts based on
simple rules (e.g. if both records have an external ID, take the external ID
that is lower).

This extension provides the framework for adding custom rules and also includes
an example of a set of custom rules. The custom rules set is chosen based on
the URL of the installation.

## TO DO

The framework and custom code is packaged together in a single extension. It
would be preferable to have one extension with the framework and package
separate extensions for each site that has custom rules.
