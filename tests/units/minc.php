<?php 

class Minc extends BaseUnit
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

        $this->assertEqual( $this->client->minc( 'f' ), count($test) );

        foreach( $test as $k => $v ){
            $this->assertEqual( $this->client->get($k), $v + 1 );
        }
    }

    public function clean(){
        $this->client->mdel('f');
    }
}

?>
