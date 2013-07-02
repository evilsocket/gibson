<?php 

require_once 'testlib.php';

$g = new Gibson();

$big = str_repeat( "a", 50000 );

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );

fail_if( $g->set( "foo", "bar" ) == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "fuu", "rar" ) == FALSE, "Unexpected SET reply" );

for( $i = 0; $i < 500; $i++ ){
	fail_if( $g->set( "big$i", $big ) != $big, "Unexpected SET reply" );
}

fail_if( $g->mdel( "big" )    		== FALSE, "Unexpected MDEL reply" );

fail_if( $g->get( "foo" ) == FALSE, "Value should be still there" );
fail_if( $g->get( "fuu" ) == FALSE, "Value should be still there" );

fail_if( $g->mdel( "f" )    		== FALSE, "Unexpected MDEL reply" );

fail_if( $g->get( "foo" ) !== FALSE, "Value should be deleted" );
fail_if( $g->get( "fuu" ) !== FALSE, "Value should be deleted" );

$g->quit();

?>