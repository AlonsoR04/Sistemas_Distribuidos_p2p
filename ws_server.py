from datetime import datetime

from spyne import Application, ServiceBase, Unicode, rpc
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication


class FechaHoraService(ServiceBase):
    """Servicio para obtener la fecha y hora actual en formato dd/mm/yyyy,hh:mm:ss"""
    @rpc(_returns=Unicode)
    def get_datetime(ctx):
        now = datetime.now() # Obtener la fecha y hora actual
        return now.strftime('%d/%m/%Y,%H:%M:%S') # Devolver la hora en el formato deseado

application = Application(
    services=[FechaHoraService],
    tns='http://tests.python-zeep.org/',
    in_protocol=Soap11(validator='lxml'),
    out_protocol=Soap11())

application = WsgiApplication(application)

if __name__ == '__main__':
    import logging
    from wsgiref.simple_server import make_server

    logging.basicConfig()
    logging.getLogger('spyne.protocol.xml')

    logging.info("listening to http://127.0.0.1:8080")
    logging.info("wsdl is at: http://localhost:8080/?wsdl")

    server = make_server('127.0.0.1', 8080, application) # Desplegar el servidor en localhost:8080
    server.serve_forever()
