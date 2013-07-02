<?php 

// Regression test for issue #5:
// The encoding doesn't get updated when a LZF encoded item is decompressed and sent in a multi set.

require_once 'testlib.php';

$big = str_repeat( 'a', 50000 );

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "big", $big )      == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "bog", $big )      == FALSE, "Unexpected SET reply" );

$multi = $g->mget( "b" );

fail_if( $multi == FALSE || is_array($multi)== FALSE || count($multi) != 2, "Unexpected MGET reply" );

$g->quit();

?>
