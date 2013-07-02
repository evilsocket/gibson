#!/usr/bin/env php
<?php

if( !class_exists('Gibson') ) die( "You will need the Gibson PHP extension to run the tests.\n" );

chdir( dirname(__FILE__) );

$shortopts = "C:S:"; 
$longopts  = array(
	"custom-server:",
	"single-test:" 
);
$options = getopt($shortopts, $longopts);

if( isset( $options['C'] ) || isset( $options['custom-server'] ) ){
	$bCustomServer = TRUE;
	define( 'GIBSON_SOCKET', isset( $options['C'] ) ? $options['C'] : $options['custom-server'] );
}
else {
	$bCustomServer = FALSE;
	define( 'GIBSON_SOCKET', '../gibson.sock' );
}

if( isset( $options['S'] ) || isset( $options['single-test'] ) ){
	$tests = array( ( isset( $options['S'] ) ? $options['S'] : $options['single-test'] ).'.test.php' );
}
else {
	$tests = glob("*.test.php" );
}

if( $bCustomServer == FALSE )
	system("cd .. && cmake . && make && ./gibson -c tests/config.test");

$passed = 0;
$failed = 0;
$start  = microtime(TRUE);

foreach( $tests as $testfile ){
	$name = strtoupper( str_replace( '.test.php', '', $testfile ) );
	
    printf( "Running %15s test ...", $name );
  
 	$t_start = microtime(TRUE);
	$output  = trim( shell_exec("php $testfile") );
  	$t_end   = microtime(TRUE);
	if( $output ){
		printf( " FAILED: %s\n", $output );
   		++$failed;  
  	}
	else { 
		printf( " PASSED ( %s )\n", format_timespan( $t_end - $t_start ) ); 
    	++$passed;
  	}
}

$end = microtime(TRUE);

if( $bCustomServer == FALSE )
	system("killall gibson");

printf( "\n%d passed, %d failed ( %s )\n", $passed, $failed, format_timespan( $end - $start ) );

if( $failed > 0 )
	exit(1);

function format_timespan( $delta ){
  $ms = $delta * 1000.0;
  if( $ms < 1000 )
    return number_format( $ms, 2 )." ms";

  else if( $ms < 60000 )
    return number_format( $ms / 1000, 2 )." s";

  else 
    return number_format( $ms / 60000, 2 )." m";
}

?>
