fwlocator
=========

This project consists of two parts:

* An Android app which detects fireworks noise and sends events to a central
  site. Time is synchronized with the server to cancel clock offset between
  phones.

* An application running on the central site receives events from three or more
  devices, and calculates the origin of the noise using multilateration.
  Detected locations are made visible using the Openstreetmap or Google maps
  API.

