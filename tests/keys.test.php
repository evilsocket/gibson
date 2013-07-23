<?php

require_once 'testlib.php';

$test = array
(
	'app:foo' => 'bar',
	'app:fuu' => 'bur',
	'app:fir' => 'bir'	
);

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );

foreach( $test as $k => $v )
	fail_if( $g->set( $k, $v ) == FALSE, "Unexpected SET reply" );

$keys = $g->keys('app:');

fail_if( !$keys || !is_array($keys) || count($keys) != 3, "Unexpected KEYS reply" );

?>
