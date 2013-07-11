<?php 

// Regression test for issue #8:
// Compression rate average is wrong ( always 0 ).

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );

for( $i = 0; $i < 1000; $i++ ){
    $big = str_repeat( generateRandomString(), rand( 500, 1000 ) );
    fail_if( $g->set( "big$i", $big )      == FALSE, "Unexpected SET reply" );
}

$stats = $g->stats();

fail_if( !is_array($stats) || !isset($stats['compr_rate_avg']), "Unexpected STATS reply" );
fail_if( $stats['compr_rate_avg'] < 90, "Compression rate should be around 98%" );

function generateRandomString($length = 10) {
    $characters = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
    $randomString = '';
    for ($i = 0; $i < $length; $i++) {
        $randomString .= $characters[rand(0, strlen($characters) - 1)];
    }
    return $randomString;
}

?>
