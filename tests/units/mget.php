<?php 

class Mget extends BaseUnit
{
    private $hash = NULL;

    public function test(){
        $test = array
        (
            'foo' => 'bar',
            'fuu' => 'bur',
            'fii' => 'bir'	
        );

        foreach( $test as $k => $v ){
            $this->assertEqual( $this->client->set( $k, $v ), $v );
        }

        $mget = $this->client->mget( 'f' );

        $this->assertIsA( $mget );
        $this->assertEqual( $mget, $test );
    }

    public function testExpired(){
        $test = array
        (
            'foo' => 'bar',
            'fuu' => 'bur',
            'fii' => 'bir'	
        );

        foreach( $test as $k => $v ){
            $this->assertEqual( $this->client->set( $k, $v, 1 ), $v );
        }

        sleep(1);

        $this->assertFalse( $this->client->mget('f') );
    }

    public function testBinary(){
        $this->hash = md5( 'foobar', true );

        $test = array
        (
            $this->hash.'foo' => 'bar',
            $this->hash.'fuu' => 'bur',
            $this->hash.'fii' => 'bir'	
        );

        foreach( $test as $k => $v ){
            $this->assertEqual( $this->client->set( $k, $v ), $v );
        }

        $mget = $this->client->mget( $this->hash );

        $this->assertIsA( $mget );
        $this->assertEqual( $mget, $test );
    }

    public function clean(){
        $this->client->mdel('f');
        if( $this->hash )
        {
            $this->client->mdel( $this->hash );
        }
    }
}

?>
