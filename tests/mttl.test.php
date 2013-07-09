<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "foo", "bar" ) == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "fuu", "bur" ) == FALSE, "Unexpected SET reply" );
fail_if( $g->set( "fii", "bir" ) == FALSE, "Unexpected SET reply" );
fail_if( $g->mttl( "f", 1 )      != 3, "Unexpected TTL reply" );

sleep(1);

fail_if( $g->get( "foo" ) !== FALSE, "Value should be expired" );
fail_if( $g->get( "fuu" ) !== FALSE, "Value should be expired" );
fail_if( $g->get( "fii" ) !== FALSE, "Value should be expired" );

?>
