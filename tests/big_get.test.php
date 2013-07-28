<?php 

require_once 'testlib.php';

$g = new Gibson();

$big = str_repeat( "a", 1000000 );

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );

fail_if( $g->set( "bg:foo", $big ) == FALSE, "Unexpected SET reply" );

fail_if( $g->get( "bg:foo" ) != $big, "Unexpected GET reply" );

?>
