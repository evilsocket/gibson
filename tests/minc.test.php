<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "1" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "fuu", "1" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->minc( "f" )            == FALSE, "Unexpected MINC reply" );
fail_if( $g->get( "foo" ) !== 2, "Value should be 2" );
fail_if( $g->get( "fuu" ) !== 2, "Value should be 2" );

?>
