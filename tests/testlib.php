<?php 

define( 'GIBSON_SOCKET', realpath( dirname(__FILE__).'/../gibson.sock' ) );

function fail_if( $cond, $mess ){
	if( $cond == TRUE )
		die( "$mess\n" );
}


?>