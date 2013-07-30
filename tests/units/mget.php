<?php 

class Mget extends BaseUnit
{
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

    public function clean(){
        $this->client->mdel('f');
    }
}

?>
