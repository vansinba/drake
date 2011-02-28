function taylorTest

load robotlib_config;
oldpath = addpath([conf.root,'/examples/Pendulum']);

order=1;

p = PendulumPlant();
t0=0;
x0=randn(2,1);
u0=0;
[xdot_taylor,dxdot_taylor]=taylor(@(t,x,u)dynamics(p,t,x,u),order,t0,x0,u0);
xdot=p.dynamics(t0,x0,u0);
dxdot=p.dynamicsGradients(t0,x0,u0,order);

if (any(abs(xdot_taylor-xdot)>1e-5)) error('dynamics don''t match'); end
% todo: write in check for h.o.t., possibly using msspolys to flatten

path(oldpath);
oldpath = addpath([conf.root,'/examples/Acrobot']);

p = AcrobotPlant();
x0=randn(4,1);
fun=@(t,x,u)dynamics(p,t,x,u);
tic
[xdot_taylor,dxdot_taylor]=taylor(fun,order,t0,x0,u0);
toc

tic
xdot=p.dynamics(t0,x0,u0);
dxdot=p.dynamicsGradients(t0,x0,u0,order);
toc

if (any(abs(xdot_taylor-xdot)>1e-5)) error('dynamics don''t match'); end
% todo: write in check for h.o.t., possibly using msspolys to flatten

gradTest(@(t,x,u)taylor(fun,1,t,x,u),t0,x0,u0,struct('tol',0.01))

path(oldpath);
