<?php 

require_once 'testlib.php';

$g = new Gibson();

$big  = str_repeat( "a", 50000 );
$big2 = str_repeat( "helloworld", 10000 ); 

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );

fail_if( $g->set( "big" , $big )  != $big, "Unexpected SET reply" );
fail_if( $g->set( "big2", $big2 ) != $big2, "Unexpected SET reply" );

$stats = $g->stats();

fail_if( !is_array( $stats ), "Unexpected STATS reply" );
fail_if( $stats['compr_rate_avg'] <= 0, "Average compression rate is wrong" );

?>
