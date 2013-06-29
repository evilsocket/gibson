<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "bar", 5 ) == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "fuu", "rar", 5 ) == FALSE, "Unexpected SET reply" );
fail_if( $g->mdel( "f" )    		== FALSE, "Unexpected MDEL reply" );
fail_if( $g->get( "foo" ) !== FALSE, "Value should be deleted" );
fail_if( $g->get( "fuu" ) !== FALSE, "Value should be deleted" );

$g->quit();

?>