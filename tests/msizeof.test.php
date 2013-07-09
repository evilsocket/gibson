<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "msa",  "bar" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "msb",  "bar" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->msizeof( "m" ) != 6, "MSIZEOF should be 6" );

?>
