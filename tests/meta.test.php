<?php 

require_once 'testlib.php';

$g = new Gibson();

$big = str_repeat( "a", 50000 );

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "eaaa",  "bar" ) == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "ebbb",  "1" )   == FALSE, "Unexpected SET reply" );
fail_if( $g->inc( "ebbb" )         == FALSE, "Unexpected INC reply" );
fail_if( $g->set( "eccc",  $big )  == FALSE, "Unexpected SET reply" );

fail_if( $g->meta( "eaaa", 'encoding' ) != Gibson::ENC_PLAIN,  "Encoding should be ENC_PLAIN" );
fail_if( $g->meta( "ebbb", 'encoding' ) != Gibson::ENC_NUMBER, "Encoding should be ENC_NUMBER" );
fail_if( $g->meta( "eccc", 'encoding' ) != Gibson::ENC_LZF, 	"Encoding should be ENC_LZF" );

?>
