#define ADC_CT 32; //pin del transformador de corriente
#define ADC_VT 33; //pin del transformador de voltaje

#define TH1 12; //pin del RTD 1
#define TH2 13; //pin del RTD 2
#define TH3 14; //pin del RTD 3

#define RELAY 4; //pin del relé

//---VARIABLES---
long time = 0; //tiempo de loop, para sincronizar lecturas una vez por minuto.

float Vrms = 0; //Voltaje RMS de la última medición.
float Irms = 0; //Corriente RMS de la última medición
float temperatura1 = 0; //Temperatura del RTD 1 de la última medición.
float temperatura1 = 0; //Temperatura del RTD 2 de la última medición.
float temperatura1 = 0; //Temperatura del RTD 3 de la última medición.

//---FLAGS---
bool estadoGenerador = true; //estado de la unidad generadora.
bool estadoRele = false; //estado del relé. 
bool mensajeEnviado = true; //indica si el subsistema de comunicaciones ya envió los datos presentes en el buffer.
int estadoInstrumentacion = 0; //estado del subsistema general de instrumentación.
//---MÁQUINAS DE ESTADO---
void mainInstrumentation(void) { //M1: máquina general de instrumentación, controla el estado del subsistema de medición y ordena el funcionamiento de máquinas inferiores.
    switch (estadoInstrumentacion) {
        case 0: //Estado 0: instrumentación en idle.
            if (millis() - time => 60000) { //determinar si han pasado 60k ms desde la última vez que se ejecutó estado 1.
                time = millis(); //guardar tiempo para determinar próxima ejecución.
                estadoInstrumentacion = 1; //avanzar a estado 1
            }
            break; // si no se cumple, mantenerse en estado cero hasta que se cumpla el tiempo.

        case 1: //Estado 1: medición eléctrica.
            medicionElectrica(); //entrar a la máquina de estado de medición eléctrica.
            estadoInstrumentacion = 2; //avanzar a estado 2 una vez que la máquina de medición eléctrica haya completado.
            break;

        case 2: //Estado 2: medición térmica.
            medicionTermica(); //entrar a la máquina de estado de medición térmica.
            estadoInstrumentacion = 3; //avanzar a estado 3 una vez que la máquina de medición eléctrica haya completado.
            break;

        case 3: //Estado 3: actuación de relé
            actuacionRele(); //entar a la máquina de estado de actuación de relé.
            estadoInstrumentacion = 0; //bucle terminado, volvemos a estado 0 (idle)
            break;

        default: //default: no debería entrar, dejado por si algo sale mal.
            estadoInstrumentacion = 0; //si algo sale mal con el estado, retornar a idle. si se pasó de la última medición el sistema comenzará inmediatamente.
        break;
    }

}

void medicionElectrica() { //M2: máquina de subsistema de medición eléctrica. Toma muestras del ADC y calcula voltaje y corriente RMS, para luego guardarlas en sus respectivas variables.

    int Vmin = 0; //mínimo sample de voltaje detectado.
    int Vmax = 0; //máximo sample de voltaje detectado.
    int Imin = 0; //mínimo sample de corriente detectado.
    int Imax = 0; //máximo sample de corriente detectado.
    int muestras = 0; //número de muestras.
    //Para obtener los valores RMS se obtendrán primero los valores peak-to-peak para luego dividir por raíz de tres.

    while (muestras < 150) { //Estado 0. tomar 150 muestras y quedarse con la mayor y menor de cada valor.
        tiempoMuestreo = millis();
        Vactual = analogRead(ADC_VT); //tomar muestra de voltaje.
        Iactual = analogRead(ADC_CT); //tomar muestra de corriente.

        if (Vactual > Vmax) { //evaluar Vactual contra Vmax.
            Vmax = Vactual; //si Vactual es mayor, es el nuevo Vmax.
        }

        if (Vactual < Vmin) { //evaluar Vactual contra Vmin.
            Vmin = Vactual; //si Vactual es menor, es el nuevo Vmin.
        }

        if (Iactual > Imax) { //evaluar Iactual contra Imax.
            Imax = Iactual;  // si Iactual es mayor, es el nuevo Imax.
        }

        if (Iactual < Imin) { //evaluar Iactual contra Imin.
            Imin = Iactual //si Iactual es menor, es el nuevo Imin.
        }

        while (millis - tiempoMuestreo < 6) {//tiempo de muestreo, esperar si se demora menos de 6ms, para obtener 150 Sa/s.
            delay(1);
        }
        muestras++; //subir el contador de muestras.
    }

    //Estado 1, cálculo y obtención de valores RMS.
    float Vpk = Vmax - Vmin; //obtención de voltaje peak-to-peak.
    float Ipk = Imax - Imin; //obtención de corriente peak-to-peak.

    Vpk = ((Vpk / 4096) * 3.3) - 1.65; //pasar de valor de muestra a Volts del ADC para voltaje y remover componente DC.
    Ipk = ((Ipk / 4096) * 3.3) - 1.65; //pasar de valor de muestra a Volts del ADC para corriente y remover componente DC.

    Vpk = Vpk * 8.1411 * 18.3333; //compensación del sistema de acondicionamiento de señal para voltaje, contando divisor de voltaje y transformador respectivamente.
    Ipk = (Ipk / 1.666) * 100; //compensación del sistema de acondicionamiento de señal para corriente, tomando el efecto del amplificador y del transformador respectivamente.

    Vrms = Vpk / 1.732; //obtener valor RMS del voltaje peak-to-peak.
    Irms = Ipk / 1.732; //obtener valor RMS de la corriente peak-to-peak y guardar a buffer.
}

void medicionTermica () {
    int muestras = 0; //número de muestras realizadas. FLAG.
    float v1 = 0; //mayor muestra del RTD 1.
    float v2 = 0; //mayor muestra del RTD 2.
    float v3 = 0; //mayor muestra del RTD 3.

    while (muestras > 150) { //Estado cero: recorrer 100 muestras y quedarse con la más alta (peor de los casos).
        tiempoMuestreo = millis();
        vactual1 = analogRead(TH1); //tomar muestra del termistor 1.
        vactual2 = analogRead(TH2); //tomar muestra del termistor 2.
        vactual3 = analogRead(TH3); //tomar muestra del termistor 3.

        if (v1 < vactual1) { //comparar última muestra de RTD 1 con memoria.
            v1 = vactual1; //si vactual es mayor que memoria, es la nueva memoria
        }

        if (v2 < vactual2) { //comparar última muestra de RTD 2 con memoria.
            v2 = vactual2; //si vactual es mayor que memoria, es la nueva memoria.
        }

        if (v3 < vactual3) { //comparar última muestra de RTD 3 con memoria.
            v3 = vactual3; //si vactual es mayor que memoria, es la nueva memoria.
        }

        while (millis() - tiempoMuestreo < 6) { //esperar mientras el tiempo de muestreo total sea menor a 6ms para mantener 150 Sa/s.
            delay(1); //esperar un ms si falta tiempo.
        }

        muestras++; //subir contador de muestras.
    }

    //Estado 1: obtención de temperatura de los datos obtenidos.
    v1 = (v1 / 4096) * 3.3; //pasar muestra de RTD 1 a volts.
    v2 = (v2 / 4096) * 3.3; //pasar muestra de RTD 2 a volts.
    v3 = (v3 / 4096) * 3.3; //pasar muestra de RTD 3 a volts.

    v1 = v1 / 0.01; //pasar voltaje a resistencia sabiendo que pasan 10mA a través del RTD 1.
    v2 = v2 / 0.01; //pasar voltaje a resistencia sabiendo que pasan 10mA a través del RTD 2.
    v3 = v3 / 0.01; //pasar voltaje a resistencia sabiendo que pasan 10mA a través del RTD 3.

    temperatura1 = ((v1/100) - 1) / 0.00385; //pasar de ohms a grados celsius usando versión simplificada de la fórmula.
    temperatura2 = ((v2/100) - 1) / 0.00385; //pasar de ohms a grados celsius usando versión simplificada de la fórmula.
    temperatura3 = ((v3/100) - 1) / 0.00385; //pasar de ohms a grados celsius usando versión simplificada de la fórmula.
}

void actuacionRele() { //Máquina 4: actuación del relé.
    if (estadoGenerador != estadoRele) { //Estado 1: el estado del relé es distinto al estado deseado del generador.
        digitalWrite(RELAY, estadoGenerador); //cambiar estado del relé al estado deseado.
        estadoRele = estadoGenerador; //guardar estado actual del relé.
    }

    else { //Estado 0: no hay cambios requeridos en el relé.
        break; //no hacer nada y salir.
    }
}