<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "bar" )    == FALSE, "Unexpected SET reply" );
fail_if( $g->del( "foo" )           == FALSE, "Unexpected DEL reply" );
fail_if( $g->get( "foo" ) !== FALSE, "Value should be deleted" );

?>
