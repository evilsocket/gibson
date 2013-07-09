<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->pconnect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );
fail_if( $g->set( "dlfoo",  "bar" )      == FALSE, "Unexpected SET reply" );
fail_if( $g->lock( "dlfoo", 10 )         == FALSE, "Unexpected LOCK reply" );
fail_if( $g->lock( "dlfoo", 10 )         == TRUE, "Value shouldn't be double lockable" );

?>
