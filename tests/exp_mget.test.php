<?php 

require_once 'testlib.php';

$test = array
(
	'mfoo' => 'bar',
	'mfuu' => 'bur',
	'mfii' => 'bir'	
);

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
foreach( $test as $k => $v )
	fail_if( $g->set( $k, $v, 1 ) == FALSE, "Unexpected SET reply" );

sleep( 1 );

fail_if( $g->mget( "m" ) != FALSE, "Items should be expired" );

$g->quit();

?>