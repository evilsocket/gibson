<?php 

define( 'GIBSON_SOCKET', realpath( dirname(__FILE__).'/../gibson.sock' ) );

function fail_if( $cond, $mess ){
	global $g;
	
	if( $cond == TRUE )
		die( "$mess ( client last error: '".$g->getLastError()."' )\n" );
}


?>