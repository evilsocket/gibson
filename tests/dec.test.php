<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "1" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->dec( "foo" )           === FALSE, "Unexpected DEC reply " );
fail_if( $g->get( "foo" ) !== 0, "Value should be 0" );

?>
