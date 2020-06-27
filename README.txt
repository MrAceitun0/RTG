Laura Beltrán 	NIA:194993 	laura.beltran02@estudiant.upf.edu
Sergi Olives	NIA:193196	sergi.olives01@estudiant.upf.edu

Gráficos en Tiempo Real - Entrega Práctica 3
Esta práctica se construye sobre la práctica anterior de la asignatura donde se implementa una pipeline de render en modo deferred.
Esta es una versión mejorada con otros algoritmos:
*Se implementa el SSAO+ 
*Se implementa irradiancia aunque no se visualiza en el render de iluminación (se puede comprovar que las probes de irradiancia funcionan correctamente y se puede visualizar la textura de irradiancia)
*Se implementa un sistema de reflexiones dónde se pueden visualizar las probes de reflexión y como se aplican en el escenario
*Se puede activar la opción de volumetric lighting para luz direccional
*Se implementa un postprocesado de tonemapping
*Sobre el prefab del coche, podemos encontrar un decal implementado
*Se implementan parallax reflections (solo se ha conseguido implementar en forward y para un plano en el suelo)

Contenido de la escena:
	Prefab de un coche (gmc)
	Prefab de una casa (brutalism)
	Suelo de piedra / Suelo reflectante rojo (solo forward)
	Agujeros de bala (decals)
	Luz direccional
	Spot light amarilla

DEBUGGER
1. Tipo de pipeline (Forward / Deferred)
En Forward:
	2. Planar reflection: activa un plano sobre el suelo que refleja lo que se encuentra sobre la escena, se pueden modificar las models de los prefabs para ver el efecto parallax
En Deferred:
	2. Show gbuffers: muestra algunos buffers usados para debugar la pipeline de deferred (color, normales, depth y ssao)
	3. Checkbox Gamma: si está activo se aplica el gamma.
	4. Blur SSAO: blurrea el buffer SSAO para aplicar SSAO+
	5. SSAO bias
	6. Computar irradiancia
		6.1. Visualizar probes de irradiancia
		6.2. Visualizar textura de irradiancia
	7. Computar reflexiones de la escena
		7.1 Mostrar probes con reflexiones
		7.2 Aplicar reflexiones en la escena
	8. Mostrar decals
	9. Activar o desactivar tonemapper con sus parámetros modificables (scale, average lum, lum white, igamma)
	10. Activar o desactivar volumetric lighting para luz direccional (se puede modificar el sample density)
También se permite debuggar prefabs y luces de la escena:
	*Camara
	*Casa (contiene dos luces emisivas que se puede ver su efecto en deferred gracias a las probes de irradiancia)
	*Coche
	*Suelo de piedras
	*Luz direccional
	*Luz spot amarilla

Otros:
*Los algoritmos estan todos implementados en deferred, se añade la opción de forward al motor para las parallax reflections y porque lo usamos para debuggar, aunque no se han implementado algoritmos en esta pipeline de render (contiene errores a causa de modificaciones durant el proceso)
*Parallax reflections solo se pueden activar en forward, no se ha conseguido implementar en deferred
*Las reflexiones de las probes funcionan correctamente, aunque parece ser que hay errores arrastrados de la práctica anterior sobre pbr y provoca efectos extraños en el coche
*La irradiancia no es aplicada a la última pasada de iluminación, aunque las probes se renderizan como se espera y las luces emisivas de la casa afectan a la irradiancia. Las probes y la textura de irradiance se ve como debería.