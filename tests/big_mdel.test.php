<?php 

require_once 'testlib.php';

$g = new Gibson();

$big = str_repeat( "a", 50000 );

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );

fail_if( $g->set( "foo", "bar" ) == FALSE, "Unexpected SET reply (foo)" );
fail_if( $g->set( "fuu", "rar" ) == FALSE, "Unexpected SET reply (fuu)" );

for( $i = 0; $i < 500; $i++ ){
    $reply = $g->set( "big$i", $big );
	fail_if( $reply != $big, "Unexpected SET reply (big$i)" );
}

fail_if( $g->mdel( "big" )    		== FALSE, "Unexpected MDEL reply" );

fail_if( $g->get( "foo" ) == FALSE, "Value should be still there" );
fail_if( $g->get( "fuu" ) == FALSE, "Value should be still there" );

fail_if( $g->mdel( "f" )    		== FALSE, "Unexpected MDEL reply" );

fail_if( $g->get( "foo" ) !== FALSE, "Value should be deleted" );
fail_if( $g->get( "fuu" ) !== FALSE, "Value should be deleted" );

?>
