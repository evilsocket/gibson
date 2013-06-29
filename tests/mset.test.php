<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "bar", 5 ) == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "fuu", "rar", 5 ) == FALSE, "Unexpected SET reply" );
fail_if( $g->mset( "f", "yeah" )    == FALSE, "Unexpected MSET reply" );
fail_if( $g->get( "foo" ) !== "yeah", "MSET failed" );
fail_if( $g->get( "fuu" ) !== "yeah", "MSET failed" );

$g->quit();

?>