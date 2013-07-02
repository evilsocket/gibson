<?php 

require_once 'testlib.php';

$g = new Gibson();

$big = str_repeat( "a", 50000 );

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "eaaa",  "bar" ) == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "ebbb",  "1" )   == FALSE, "Unexpected SET reply" );
fail_if( $g->inc( "ebbb" )         == FALSE, "Unexpected INC reply" );
fail_if( $g->set( "eccc",  $big )  == FALSE, "Unexpected SET reply" );

fail_if( $g->encof( "eaaa" ) != Gibson::ENC_PLAIN,  "ENCOF should be ENC_PLAIN" );
fail_if( $g->encof( "ebbb" ) != Gibson::ENC_NUMBER, "ENCOF should be ENC_NUMBER" );
fail_if( $g->encof( "eccc" ) != Gibson::ENC_LZF, 	"ENCOF should be ENC_LZF" );

$g->quit();

?>
