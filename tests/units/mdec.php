<?php 

class Mdec extends BaseUnit
{
    public function test(){
        $test = array
        (
            'foo' => '1',
            'fuu' => '1'
        );

        foreach( $test as $k => $v ){
            $this->assertEqual( $this->client->set( $k, $v ), $v );
        }

        $this->assertEqual( $this->client->mdec( 'f' ), count($test) );

        foreach( $test as $k => $v ){
            $this->assertEqual( $this->client->get($k), $v - 1 );
        }
    }

    public function testNaN(){
        $test = array
        (
            'foo' => '1',
            'fuu' => '1',
            'fu'  => 'ck'
        );

        foreach( $test as $k => $v ){
            $this->assertEqual( $this->client->set( $k, $v ), $v );
        }

        $this->assertEqual( $this->client->mdec( 'f' ), count($test) - 1 );

        foreach( $test as $k => $v ){
            if( $k == 'fu' )
                $this->assertEqual( $this->client->get($k), $v );

            else
                $this->assertEqual( $this->client->get($k), $v - 1 );
        }
    }

    public function clean(){
        $this->client->mdel('f');
    }
}

?>
