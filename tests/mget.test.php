<?php 

require_once 'testlib.php';

$test = array
(
	'foo' => 'bar',
	'fuu' => 'bur',
	'fii' => 'bir'	
);

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
foreach( $test as $k => $v )
	fail_if( $g->set( $k, $v ) == FALSE, "Unexpected SET reply" );

fail_if( array_diff( $g->mget( "f" ), $test ) != array(), "Unexpected MGET reply" );

$g->quit();

?>