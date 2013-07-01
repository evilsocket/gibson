<?php 

require_once 'testlib.php';

$g = new Gibson();

fail_if( $g->connect(GIBSON_SOCKET) == FALSE, "Could not connect to test instance" );

$values = array
(
	"This is the Euro symbol '€'.",
	"Télécom",
	"Weiß, Goldmann, Göbel, Weiss, Göthe, Goethe und Götz"	
);

foreach( $values as $value ){
	fail_if( $g->set( "utf", $value  ) != $value, "Unexpected SET reply" );
	fail_if( $g->get( "utf" ) 		   != $value, "Unexpected SET reply" );
}

$g->quit();

?>
