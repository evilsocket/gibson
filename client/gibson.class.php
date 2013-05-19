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

	const CMD_SET    = 1;
	const CMD_TTL    = 2;
	const CMD_GET    = 3;
	const CMD_DEL    = 4;
	const CMD_INC    = 5;
	const CMD_DEC    = 6;
	const CMD_LOCK   = 7;
	const CMD_UNLOCK = 8;
	const CMD_SEARCH = 9;

	const CMD_QUIT = 0xFF;

	const REPL_ERR 		     = 0;
	const REPL_ERR_NOT_FOUND = 1;
	const REPL_ERR_NAN 	     = 2;
	const REPL_ERR_MEM   	 = 3;
	const REPL_ERR_LOCKED    = 4;
	const REPL_OK  		     = 5;
	const REPL_VAL 		     = 6;

	public function __construct( $address ){
		$this->sd 		  = fsockopen( $address, NULL );
		$this->last_error = self::REPL_OK;
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
	
	public function set( $key, $value ){
		return $this->sendCommandAssert( self::CMD_SET, "$key $value", self::REPL_VAL );
	}

	public function ttl( $key, $ttl ){
		return $this->sendCommandAssert( self::CMD_TTL, "$key $ttl", self::REPL_OK );
	}

	public function get( $key ){
		return $this->sendCommandAssert( self::CMD_GET, $key, self::REPL_VAL );	
  	}

	public function del( $key ){
		return $this->sendCommandAssert( self::CMD_DEL, $key, self::REPL_OK );
	}

	public function inc( $key ){
		return $this->sendCommandAssert( self::CMD_INC, $key, self::REPL_VAL );
 	}
  
	public function dec( $key ){
		return $this->sendCommandAssert( self::CMD_DEC, $key, self::REPL_VAL );
  	}
  
	public function lock( $key ){
  		return $this->sendCommandAssert( self::CMD_LOCK, $key, self::REPL_OK );
  	}
  
	public function unlock( $key ){
		return $this->sendCommandAssert( self::CMD_UNLOCK, $key, self::REPL_OK );
  	}
  
  	public function quit(){
  		return $this->sendCommandAssert( self::CMD_QUIT, '', self::REPL_OK );
 	}
 	
 	private function sendCommandAssert( $op, $cmd, $code ){
 		list( $opcode, $reply ) = $this->sendCommand( $op, $cmd );
 	
 		if( $opcode == $code )
 			return $code == self::REPL_VAL ? $reply : TRUE;
 	
 		else
 		{
 			$this->last_error = $opcode;
 			return FALSE;
 		}
 	}

	private function sendCommand( $cmd, $data ){
	    $cmd = pack( 'S', $cmd ).$data;
	    $cmd = pack( 'I', strlen($cmd) ).$cmd;
	    @fwrite( $this->sd, $cmd );
	
	    $reply = '';
	    $data  = '';
	
	    $opcode = fread( $this->sd, 2 );
	    $opcode = unpack( 'S', $opcode );
	    $opcode = $opcode[1];
		
	    $length = fread( $this->sd, PHP_INT_SIZE );
	    $length = unpack( 'I', $length );
	    $length = $length[1];
		
	    $read = 0;
	    
	    while( $read < $length ){
	    	$reply .= fread( $this->sd, $length - $read );
	    	$read = strlen($reply);
	    }
	    	
	    return array( $opcode, $reply );
	}
}

?>
