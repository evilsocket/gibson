<?php
/*
 * Copyright (c) 2013, Simone Margaritelli <evilsocket at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Gibson nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
		* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
		* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

class Gibson
{
	private $sd 		= NULL;
	private $last_error = 0;
	
	// single
	const CMD_SET     = 1;
	const CMD_TTL     = 2;
	const CMD_GET     = 3;
	const CMD_DEL     = 4;
	const CMD_INC     = 5;
	const CMD_DEC     = 6;
	const CMD_LOCK    = 7;
	const CMD_UNLOCK  = 8;
	// multi
	const CMD_MSET    = 9;
	const CMD_MTTL    = 10;
	const CMD_MGET    = 11;
	const CMD_MDEL    = 12;
	const CMD_MINC    = 13;
	const CMD_MDEC    = 14;
	const CMD_MLOCK   = 15;
	const CMD_MUNLOCK = 16;

	const CMD_QUIT = 0xFF;

	const REPL_ERR 		     = 0;
	const REPL_ERR_NOT_FOUND = 1;
	const REPL_ERR_NAN 	     = 2;
	const REPL_ERR_MEM   	 = 3;
	const REPL_ERR_LOCKED    = 4;
	const REPL_OK  		     = 5;
	const REPL_VAL 		     = 6;
	const REPL_KVAL			 = 7;
	
	public function __construct( $address ){
		$this->sd 		  = @fsockopen( $address, NULL );
		$this->last_error = self::REPL_OK;
		$this->unpackers  = array
		(
			self::CMD_INC  => 'I',
			self::CMD_DEC  => 'I',
			self::CMD_MSET => 'I',
			self::CMD_MTTL => 'I',
			self::CMD_MDEL => 'I',
			self::CMD_MINC => 'I',
			self::CMD_MDEC => 'I',
			self::CMD_MLOCK => 'I',
			self::CMD_MUNLOCK => 'I'
		);
	}
	
	public function getLastError(){
		switch( $this->last_error )
		{
			case self::REPL_ERR 	      : return 'Generic error.'; break;
			case self::REPL_ERR_NOT_FOUND : return 'Invalid key, item not found'; break;
			case self::REPL_ERR_NAN		  : return 'Invalid value, not a number'; break;
			case self::REPL_ERR_MEM       : return 'Gibson server is out of memory'; break;
			case self::REPL_ERR_LOCKED    : return 'The item is locked'; break;
			default:
				return 'no error';
		}
	}
	
	public function set( $key, $value, $ttl = 0 ){
		$reply = $this->sendCommandAssert( self::CMD_SET, "$key $value", self::REPL_VAL );
		if( $reply !== FALSE && $ttl > 0 )
			$this->ttl( $key, $ttl );
		
		return $reply;
	}
	
	public function mset( $expr, $value ){
		return $this->sendCommandAssert( self::CMD_MSET, "$expr $value", self::REPL_VAL );
	}

	public function ttl( $key, $ttl ){
		return $this->sendCommandAssert( self::CMD_TTL, "$key $ttl", self::REPL_OK );
	}
	
	public function mttl( $expr, $ttl ){
		return $this->sendCommandAssert( self::CMD_MTTL, "$expr $ttl", self::REPL_VAL );
	}

	public function get( $key ){
		return $this->sendCommandAssert( self::CMD_GET, $key, self::REPL_VAL );	
  	}
  	
  	public function mget( $expr ){
  		return $this->sendCommandAssert( self::CMD_MGET, $expr, self::REPL_KVAL );
  	}

	public function del( $key ){
		return $this->sendCommandAssert( self::CMD_DEL, $key, self::REPL_OK );
	}
	
	public function mdel( $expr ){
		return $this->sendCommandAssert( self::CMD_MDEL, $expr, self::REPL_VAL );
	}

	public function inc( $key ){
		return $this->sendCommandAssert( self::CMD_INC, $key, self::REPL_VAL );
 	}
 	
 	public function minc( $expr ){
 		return $this->sendCommandAssert( self::CMD_MINC, $expr, self::REPL_VAL );
 	}
  
	public function dec( $key ){
		return $this->sendCommandAssert( self::CMD_DEC, $key, self::REPL_VAL );
  	}
  	
  	public function mdec( $expr ){
  		return $this->sendCommandAssert( self::CMD_MDEC, $expr, self::REPL_VAL );
  	}
  
	public function lock( $key, $time ){
  		return $this->sendCommandAssert( self::CMD_LOCK, "$key $time", self::REPL_OK );
  	}
  	
  	public function mlock( $expr, $time ){
  		return $this->sendCommandAssert( self::CMD_MLOCK, "$expr $time", self::REPL_VAL );
  	}
  
	public function unlock( $key ){
		return $this->sendCommandAssert( self::CMD_UNLOCK, $key, self::REPL_OK );
  	}
  	
  	public function munlock( $expr ){
  		return $this->sendCommandAssert( self::CMD_MUNLOCK, $expr, self::REPL_VAL );
  	}
  
  	public function quit(){
  		return $this->sendCommandAssert( self::CMD_QUIT, '', self::REPL_OK );
 	}
 	
 	private static function unpackKeyValueSet( $data ){
 		$set = array();

 		$elements = unpack( 'I', $data );
 		$elements = $elements[1];
 		 		
 		$data = substr( $data, PHP_INT_SIZE );
 		
 		for( $i = 0; $i < $elements; $i++ ){
 			$klen = unpack( 'I', $data );
 			$klen = $klen[1];
 			$data = substr( $data, PHP_INT_SIZE );
 		
 			$key = substr( $data, 0, $klen );
 			$data = substr( $data, $klen );
 		
 			$vlen = unpack( 'I', $data );
 			$vlen = $vlen[1];
 			$data = substr( $data, PHP_INT_SIZE );
 			$val  = substr( $data, 0, $vlen );
 			$data = substr( $data, $vlen );
 		
 			$set[ $key ] = $val;
 		}
 		
 		return $set;
 	}
 	
 	private function sendCommandAssert( $op, $cmd, $code ){
 		list( $opcode, $reply ) = $this->sendCommand( $op, $cmd );
 	
 		if( $opcode == $code )
 		{
 			if( $code == self::REPL_VAL )
 			{
 				// any specific unpacker set ?
 				if( isset( $this->unpackers[ $op ] ) )
 				{
 					$reply = unpack( $this->unpackers[ $op ], $reply );
 					$reply = $reply[1];
 				}
 				
 				return $reply;
 			}
 			else if( $code == self::REPL_KVAL )
 			{
 				return self::unpackKeyValueSet( $reply );
 			}
 			else 
 				return TRUE;
 		}
 		else
 		{
 			$this->last_error = $opcode;
 			return FALSE;
 		}
 	}

 	private function readShort(){
 		$short = fread( $this->sd, 2 );
 		$short = unpack( 'S', $short );
 		$short = $short[1];
 		
 		return $short;
 	}
 	
 	private function readInt(){
 		$int = fread( $this->sd, PHP_INT_SIZE );
 		$int = unpack( 'I', $int );
 		$int = $int[1];
 		
 		return $int;
 	}
 	 	
	private function sendCommand( $cmd, $data ){
		
		if( $this->sd )
		{
		    $cmd = pack( 'S', $cmd ).$data;
		    $cmd = pack( 'I', strlen($cmd) ).$cmd;
		    @fwrite( $this->sd, $cmd );
		
		    $reply = '';
		    $data  = '';
		
		    $opcode = $this->readShort();
		    $length = $this->readInt();

		    $read = 0;
		    
		    while( $read < $length ){
		    	$reply .= fread( $this->sd, $length - $read );
		    	$read = strlen($reply);
		    }
		    	
		    return array( $opcode, $reply );
		}
		else
			return array( self::REPL_ERR, NULL );
	}
}

?>
