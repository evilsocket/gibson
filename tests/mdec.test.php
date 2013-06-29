<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "1" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "fuu", "1" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->mdec( "f" )            == FALSE, "Unexpected MDEC reply" );
fail_if( $g->get( "foo" ) !== 0, "Value should be 0" );
fail_if( $g->get( "fuu" ) !== 0, "Value should be 0" );

$g->quit();

?>