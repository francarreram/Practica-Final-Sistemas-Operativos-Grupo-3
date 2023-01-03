#!/bin/bash


function mostrar(){
        cat practicaFinal.c
}


function compilar(){
        gcc practicaFinal.c -o final
}

function ejecutar(){
        if test -f final
        then
                if test -x final
                then
                                echo -e "Introduzca el numero de asistentes de vuelo\n"

                                read args

                                if test $args -le 1
                                        then echo -e "Debe haber mas de 1 asistente de vuelo\n"
                                else
                                        ./final $args

                                fi

                else
                        echo -e "El archivo no tiene permisos de ejecucion\n"

                fi
        else
                echo -e "Primero debe compilar el archivo\n"
        fi
}

while true
do
echo -e "Seleccione una opcion:\n"
echo -e "1.-Mostrar el codigo\n"
echo -e "2.-Compilar el codigo\n"
echo -e "3.-Ejecutar el codigo\n"
echo -e "4.-Salir del programa\n"


read input

        case $input in

                1)mostrar
                 ;;
                2)compilar
                 ;;
                3)ejecutar
                 ;;
                4)break
                 ;;
                *) echo -e "Escoja una de las 4 opciones"

        esac
done


#final

