<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );

for( $i = 0; $i < 100; $i++ ){
    $key = "binkey".md5("binkey$i",true);
    $key = str_replace( ' ', '_', $key ); // space is used to split key and value
    $val = "value$i";

    fail_if( $g->set( $key, $val  ) != $val, "Unexpected SET reply for key $i" );
    fail_if( $g->get( $key ) != $val, "Unexpected GET reply for key $i" );
}

fail_if( $g->mdel( "binkey" ) != 100, "Unexpected MDEL reply" );

?>
