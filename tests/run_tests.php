#!/usr/bin/env php
<?php

if( !class_exists('Gibson') ) die( "You will need the Gibson PHP extension to run the tests.\n" );

chdir( dirname(__FILE__) );


$shortopts = "C:S:"; 
$longopts  = array
(
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

include_once 'BaseUnit.class.php';

if( isset( $options['S'] ) || isset( $options['single-test'] ) ){
	$tests = array( "units/".( isset( $options['S'] ) ? $options['S'] : $options['single-test'] ).'.php' );
}
else {
	$tests = glob("units/*.php" );
}

if( $bCustomServer == FALSE )
	system("cd .. && ./gibson -c tests/config.test");

$passed = 0;
$failed = 0;
$start  = microtime(TRUE);

foreach( $tests as $testfile ){
    $filename  = basename($testfile);
	$name      = strtoupper( str_replace( '.php', '', $filename ) );
    $classname = str_replace( '.php', '', $filename );
    $classname = str_replace( '_', ' ', $classname );
    $classname = str_replace( ' ', '', ucwords($classname) );

    printf( "Running %15s Unit :", $name );

    include $testfile;

    try
    {
        $inst = new $classname();

        $t_start = microtime(TRUE);

        $inst->run();
        
  	    $t_end = microtime(TRUE);
        
        ++$passed;
        
        printf( " \033[32mPASSED\033[m \033[01;30m( %s )\033[m\n", format_timespan( $t_end - $t_start ) ); 
    }
    catch( Exception $e )
    {
        ++$failed;

		printf( " \033[31mFAILED: %s\033[m\n", $e->getMessage() );
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
